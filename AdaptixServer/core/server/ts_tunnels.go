package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsTunnelList() (string, error) {
	var tunnels []adaptix.TunnelData
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnels = append(tunnels, value.(*Tunnel).Data)
		return true
	})

	jsonTunnel, err := json.Marshal(tunnels)
	if err != nil {
		return "", err
	}
	return string(jsonTunnel), nil
}

func (ts *Teamserver) TsTunnelClientStart(AgentId string, Listen bool, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {
	var (
		taskId   string
		tunnelId string
		err      error
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", fmt.Errorf("agent '%v' does not exist", AgentId)
	}
	agent, _ := value.(*Agent)
	if agent.Active == false {
		return "", fmt.Errorf("agent '%v' not active", AgentId)
	}

	commandline := ""
	message := ""
	switch Type {

	case TUNNEL_SOCKS4:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks4 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks4 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_SOCKS5:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_SOCKS5_AUTH:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_WS_SOCKS5:
		if !Listen {
			return "", errors.New("WebSocket SOCKS5 tunnel requires server-side listening")
		}
		commandline = fmt.Sprintf("[from browser] ws_socks5 start %v:%v", Lhost, Lport)
		message = fmt.Sprintf("WebSocket SOCKS5 tunnel created on '%v:%v'\n"+
			"Waiting for Agent to establish WebSocket connection...\n"+
			"Execute 'wstunnel start <url>' command on the agent.", Lhost, Lport)

	case TUNNEL_LPORTFWD:
		if Listen {
			commandline = fmt.Sprintf("[from browser] local_port_fwd start %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on %v:%v to %v:%v", Lhost, Lport, Thost, Tport)
		} else {
			commandline = fmt.Sprintf("[from browser] local_port_fwd (client) start on %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on (client '%v') %v:%v to %v:%v", Client, Lhost, Lport, Thost, Tport)
		}

	case TUNNEL_RPORTFWD:
		if Listen {
			commandline = fmt.Sprintf("[from browser] reverse_port_fwd start %v %v:%v", Lport, Thost, Tport)
			message = fmt.Sprintf("Starting reverse port forwarding %v to %v:%v", Lport, Thost, Tport)
		} else {

		}

	default:
		return "", errors.New("unknown tunnel type")
	}

	if Listen {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, "", Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return "", err
		}
		taskId, err = ts.TsTunnelStart(tunnelId)
		if err != nil {
			return "", err
		}

	} else {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, Client, Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return "", err
		}

		value, ok := ts.tunnels.Get(tunnelId)
		if !ok {
			return "", errors.New("tunnel not found")
		}
		tunnel, _ := value.(*Tunnel)
		tunnel.Active = true
		tunnel.TaskId, _ = krypt.GenerateUID(8)

		packet := CreateSpTunnelCreate(tunnel.Data)
		ts.TsSyncAllClients(packet)

		ts.TsEventTunnelAdd(tunnel)
	}

	taskData := adaptix.TaskData{
		TaskId:      taskId,
		Type:        TYPE_TUNNEL,
		Sync:        true,
		Message:     message,
		MessageType: CONSOLE_OUT_SUCCESS,
		ClearText:   "",
	}

	// 对于 WebSocket SOCKS5，检查是否有 token，如果有则更新 ClearText
	if Type == TUNNEL_WS_SOCKS5 && Listen {
		value, ok := ts.tunnels.Get(tunnelId)
		if ok {
			tunnel, _ := value.(*Tunnel)
			if tunnel.wsToken != "" && tunnel.Data.Info != "" {
				// Info 字段存储了 WebSocket URL
				taskData.ClearText = fmt.Sprintf("\nExecute BOF:\n  execute bof wstunnel.x64.o %s\n", tunnel.Data.Info)
			}
		}
	}

	ts.TsTaskCreate(AgentId, commandline, Client, taskData)

	return tunnelId, nil
}

func (ts *Teamserver) TsTunnelClientNewChannel(TunnelData string, wsconn *websocket.Conn) error {

	data, err := base64.StdEncoding.DecodeString(TunnelData)
	if err != nil {
		return errors.New("invalid tunnel data")
	}

	d := strings.Split(string(data), "|")
	if len(d) != 5 {
		return errors.New("invalid tunnel data")
	}

	tunnelId := d[0]
	channelId := d[1]
	mode := d[2]
	host := d[3]
	tPort := d[4]

	value, ok := ts.tunnels.Get(tunnelId)
	if !ok {
		return errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	cid, err := strconv.ParseInt(channelId, 16, 64)
	if err != nil {
		return errors.New("channelId not supported")
	}

	port := 0
	if tunnel.Type == TUNNEL_SOCKS4 || tunnel.Type == TUNNEL_SOCKS5 || tunnel.Type == TUNNEL_SOCKS5_AUTH {
		port, err = strconv.Atoi(tPort)
		if err != nil {
			return errors.New("Invalid port number")
		}
		if port < 1 || port > 65535 {
			return errors.New("Invalid port number")
		}
		if host == "" {
			return errors.New("Invalid host")
		}
	}

	if tunnel.Type == TUNNEL_SOCKS5 || tunnel.Type == TUNNEL_SOCKS5_AUTH {
		if mode != "tcp" && mode != "udp" {
			return errors.New("invalid mode")
		}
	}

	go handleTunChannelCreateClient(agent, tunnel, wsconn, int(cid), host, port, mode)

	return nil
}

func (ts *Teamserver) TsTunnelClientSetInfo(TunnelId string, Info string) error {
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.Data.Info = Info

	packet := CreateSpTunnelEdit(tunnel.Data)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTunnelClientStop(TunnelId string, Client string) error {
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.Data.Client == "" {
		_ = ts.TsTunnelStop(TunnelId)
		return nil
	}

	if tunnel.Data.Client == Client {
		value, ok = ts.tunnels.GetDelete(TunnelId)
		if !ok {
			return errors.New("tunnel Not Found")
		}
		tunnel, _ = value.(*Tunnel)

		tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
			tunChannel, _ := valueConn.(*TunnelChannel)
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}

			tunChannel.pwTun.Close()
			tunChannel.prTun.Close()
			tunChannel.pwSrv.Close()
			tunChannel.prSrv.Close()

			return true
		})

		packet := CreateSpTunnelDelete(tunnel.Data)
		ts.TsSyncAllClients(packet)

		taskData := adaptix.TaskData{
			TaskId:     tunnel.TaskId,
			Completed:  true,
			FinishDate: time.Now().Unix(),
		}

		ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)
		return nil
	}

	return errors.New("The tunnel is running on another client's side, you are not allowed to perform this operation.")
}

/// Tunnel Start

func (ts *Teamserver) TsTunnelStart(TunnelId string) (string, error) {

	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return "", errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	if tunnel.Type == TUNNEL_WS_SOCKS5 {
		tunnel.wsToken = ts.AdaptixServer.WsTunnelRegisterPending(agent.Data.Id, 10*time.Minute)
		if tunnel.wsToken != "" {
			logs.Info("", "[WS-Agent] Pending token for agent %s: %s", agent.Data.Id, tunnel.wsToken)

			// 使用 Agent 的 ExternalIP 作为 WebSocket 回连地址
			// Agent 通过 ExternalIP 回连到 Server
			serverAddr := agent.Data.ExternalIP
			if serverAddr == "" {
				// Fallback: 使用 Server 配置
				serverAddr = ts.Profile.Server.Interface
				if serverAddr == "0.0.0.0" || serverAddr == "" {
					serverAddr = "127.0.0.1"
				}
			}

			serverPort := ts.Profile.Server.Port
			endpoint := ts.Profile.Server.Endpoint
			if endpoint == "" {
				endpoint = "/api"
			}

			// 判断使用 ws 还是 wss（根据端口判断）
			protocol := "wss"
			if serverPort == 80 {
				protocol = "ws"
			}

			wsUrl := fmt.Sprintf("%s://%s:%d%s/ws/agent-tunnel?agent_id=%s&token=%s",
				protocol, serverAddr, serverPort, endpoint, agent.Data.Id, tunnel.wsToken)

			// 将 WebSocket URL 存储在 tunnel.Data.Info 中
			tunnel.Data.Info = wsUrl

			logs.Info("", "[WS-Agent] WebSocket URL for agent %s: %s (using agent ExternalIp)", agent.Data.Id, wsUrl)
		}
	}

	if tunnel.Type == TUNNEL_RPORTFWD {

		id, _ := strconv.ParseInt(TunnelId, 16, 64)
		port, _ := strconv.Atoi(tunnel.Data.Port)
		taskData := tunnel.handlerReverse(int(id), port)
		tunnelManageTask(agent, taskData)

	} else if tunnel.Type != TUNNEL_WS_SOCKS5 {
		// TUNNEL_WS_SOCKS5 不在这里启动监听器，等待 WebSocket 连接建立后再启动

		address := tunnel.Data.Interface + ":" + tunnel.Data.Port
		listener, err := net.Listen("tcp", address)
		if err != nil {
			ts.tunnels.Delete(TunnelId)
			return "", err
		}
		tunnel.listener = listener

		go func() {
			for {
				var conn net.Conn
				conn, err = tunnel.listener.Accept()
				if err != nil {
					return
				}
				go handleTunChannelCreate(ts, agent, tunnel, conn)
			}
		}()

		time.Sleep(300 * time.Millisecond)

		if err != nil {
			ts.tunnels.Delete(TunnelId)
			return "", err
		}
	}

	// 对于 TUNNEL_WS_SOCKS5，只有在 WebSocket 连接建立后才设置 Active=true
	if tunnel.Type != TUNNEL_WS_SOCKS5 {
		tunnel.Active = true
	}
	tunnel.TaskId, _ = krypt.GenerateUID(8)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	ts.TsEventTunnelAdd(tunnel)

	return tunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelCreate(AgentId string, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	tunnelData := adaptix.TunnelData{
		AgentId:  agent.Data.Id,
		Computer: agent.Data.Computer,
		Username: agent.Data.Username,
		Process:  agent.Data.Process,
		Info:     Info,
		Client:   Client,
	}

	lport := strconv.Itoa(Lport)
	tport := strconv.Itoa(Tport)

	switch Type {

	case TUNNEL_SOCKS4:
		tunnelData.Type = "SOCKS4 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5:
		tunnelData.Type = "SOCKS5 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5_AUTH:
		tunnelData.Type = "SOCKS5 Auth proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.AuthUser = AuthUser
		tunnelData.AuthPass = AuthPass

	case TUNNEL_LPORTFWD:
		tunnelData.Type = "Local port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"lportfwd"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	case TUNNEL_WS_SOCKS5:
		tunnelData.Type = "WebSocket SOCKS5"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"ws-socks5"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_RPORTFWD:
		tunnelData.Type = "Reverse port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"rportfwd"+lport)))
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	default:
		return "", errors.New("invalid tunnel type")
	}

	fConnTCP, fConnUDP, fWriteTCP, fWriteUDP, fClose, fReverse, err := ts.Extender.ExAgentTunnelCallbacks(agent.Data, Type)
	if err != nil {
		return "", err
	}

	tunnel := &Tunnel{
		connections: safe.NewMap(),
		Data:        tunnelData,
		Type:        Type,
		Active:      false,

		handlerConnectTCP: fConnTCP,
		handlerConnectUDP: fConnUDP,
		handlerWriteTCP:   fWriteTCP,
		handlerWriteUDP:   fWriteUDP,
		handlerClose:      fClose,
		handlerReverse:    fReverse,
	}

	value, ok = ts.tunnels.Get(tunnel.Data.TunnelId)
	if ok {
		t, _ := value.(*Tunnel)
		if t.Active {
			return "", errors.New("Tunnel already active")
		} else {
			ts.tunnels.Delete(tunnel.Data.TunnelId)
		}
	}

	ts.tunnels.Put(tunnel.Data.TunnelId, tunnel)

	return tunnel.Data.TunnelId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS4, Info, Lhost, Lport, "", "", 0, "", "")
}

func (ts *Teamserver) TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error) {
	if UseAuth {
		return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS5_AUTH, Info, Lhost, Lport, "", "", 0, Username, Password)
	} else {
		return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS5, Info, Lhost, Lport, "", "", 0, "", "")
	}
}

func (ts *Teamserver) TsTunnelCreateWsSocks5(AgentId string, Info string, Lhost string, Lport int) (string, string, error) {
	// 创建 WebSocket SOCKS5 隧道并返回隧道ID和WebSocket URL
	tunnelId, err := ts.TsTunnelCreate(AgentId, TUNNEL_WS_SOCKS5, Info, Lhost, Lport, "", "", 0, "", "")
	if err != nil {
		return "", "", err
	}

	// 启动隧道（生成 WebSocket URL）
	_, err = ts.TsTunnelStart(tunnelId)
	if err != nil {
		return "", "", err
	}

	// 从隧道对象中获取 WebSocket URL
	tunnelObj, ok := ts.tunnels.Get(tunnelId)
	if !ok {
		return "", "", fmt.Errorf("tunnel not found after creation")
	}
	tunnel := tunnelObj.(*Tunnel)

	return tunnelId, tunnel.Data.Info, nil
}

func (ts *Teamserver) TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_LPORTFWD, Info, Lhost, Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelCreateRportfwd(AgentId string, Info string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_RPORTFWD, Info, "", Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelUpdateRportfwd(tunnelId int, result bool) (string, string, error) {
	var (
		tunnel *Tunnel
		value  interface{}
		ok     bool
	)
	tunId := fmt.Sprintf("%08x", tunnelId)

	if result == true {
		value, ok = ts.tunnels.Get(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

			packet := CreateSpTunnelCreate(tunnel.Data)
			ts.TsSyncAllClients(packet)

			ts.TsEventTunnelAdd(tunnel)

			message := fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)

			return tunnel.TaskId, message, nil
		}
	} else {
		value, ok = ts.tunnels.GetDelete(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

			taskData := adaptix.TaskData{
				TaskId:      tunnel.TaskId,
				MessageType: CONSOLE_OUT_ERROR,
				Message:     "Reverse port forward failed",
				FinishDate:  time.Now().Unix(),
				Completed:   true,
			}

			ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

			return tunnel.TaskId, "", errors.New("reverse port forward failed")
		}
	}
	return "", "", errors.New("tunnel not found")
}

/// Tunnel Stop

func (ts *Teamserver) TsTunnelStop(TunnelId string) error {
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.listener != nil {
		_ = tunnel.listener.Close()
	}

	tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
		tunChannel, _ := valueConn.(*TunnelChannel)
		if tunnel.Data.Client == "" {
			if tunChannel.conn != nil {
				_ = tunChannel.conn.Close()
			}
		} else {
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}
		}

		tunChannel.pwTun.Close()
		tunChannel.prTun.Close()
		tunChannel.pwSrv.Close()
		tunChannel.prSrv.Close()

		return true
	})

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClients(packet)

	taskData := adaptix.TaskData{
		TaskId:     tunnel.TaskId,
		Completed:  true,
		FinishDate: time.Now().Unix(),
	}

	ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

	ts.TsEventTunnelRemove(tunnel)

	return nil
}

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

func (ts *Teamserver) TsTunnelStopLportfwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "lportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

func (ts *Teamserver) TsTunnelStopRportfwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "rportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	rawTaskData := tunnel.handlerClose(int(id))
	tunnelManageTask(agent, rawTaskData)

	_ = ts.TsTunnelStop(TunnelId)
}

/// Connection

func (ts *Teamserver) TsTunnelChannelExists(channelId int) bool {
	var (
		tunnel *Tunnel
		ok     bool
	)

	cid := strconv.Itoa(channelId)
	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		ok = tunnel.connections.Contains(cid)
		if ok {
			return false
		}
		return true
	})

	return ok
}

func (ts *Teamserver) TsTunnelGetPipe(AgentId string, channelId int) (*io.PipeReader, *io.PipeWriter, error) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if !ok {
		return nil, nil, errors.New("tunnel connection not found")
	}

	return tunChannel.prSrv, tunChannel.pwTun, nil
}

func (ts *Teamserver) TsTunnelConnectionData(channelId int, data []byte) {
	var (
		tunnel     *Tunnel
		valueConn  interface{}
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		go tunChannel.pwTun.Write(data)
	}
}

func (ts *Teamserver) TsTunnelGetWsToken(tunnelId string) (string, error) {
	value, ok := ts.tunnels.Get(tunnelId)
	if !ok {
		return "", errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)
	if tunnel.Type != TUNNEL_WS_SOCKS5 {
		return "", errors.New("tunnel is not WebSocket SOCKS5 type")
	}
	return tunnel.wsToken, nil
}

func (ts *Teamserver) TsTunnelWsAgentAck(agentId string, channelID uint32) {
	logs.Info("", "[WS-Agent] Received ACK from agent %s for channel %d", agentId, channelID)
	_, tunChannel := ts.findWsChannel(agentId, channelID)
	if tunChannel == nil {
		logs.Debug("", "[WS-Agent] Ack for unknown channel agent=%s channel=%d", agentId, channelID)
		return
	}

	tunChannel.wsSignalReady()
	logs.Info("", "[WS-Agent] Channel %d marked as ready", channelID)
}

func (ts *Teamserver) TsTunnelWsAgentData(agentId string, channelID uint32, data []byte) {
	tunnel, tunChannel := ts.findWsChannel(agentId, channelID)
	if tunChannel == nil {
		logs.Debug("", "[WS-Agent] Data for unknown channel agent=%s channel=%d", agentId, channelID)
		return
	}

	select {
	case <-tunChannel.wsClosed:
		return
	default:
	}

	if tunChannel.conn == nil {
		logs.Error("", "[WS-Agent] Connection is nil for channel %d", channelID)
		return
	}

	logs.Debug("", "[WS-Agent] Received %d bytes from agent (channel %d), forwarding to client", len(data), channelID)
	if _, err := tunChannel.conn.Write(data); err != nil {
		logs.Error("", "[WS-Agent] Write to client failed agent=%s channel=%d err=%v", agentId, channelID, err)
		ts.closeWsTunnelChannel(tunnel, tunChannel, true)
	}
}

func (ts *Teamserver) TsTunnelWsAgentClose(agentId string, channelID uint32) {
	tunnel, tunChannel := ts.findWsChannel(agentId, channelID)
	if tunChannel == nil {
		return
	}

	ts.closeWsTunnelChannel(tunnel, tunChannel, false)
}

func (ts *Teamserver) TsTunnelWsAgentSessionOpened(agentId string) {
	logs.Info("", "[WS-Agent] WebSocket session opened for agent %s, starting SOCKS5 listener if needed", agentId)

	// 查找该 Agent 的 WebSocket SOCKS5 隧道
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel, ok := value.(*Tunnel)
		if !ok {
			return true
		}
		if tunnel.Type != TUNNEL_WS_SOCKS5 || tunnel.Data.AgentId != agentId {
			return true
		}
		// 如果监听器还没有启动，启动它
		if tunnel.listener == nil && !tunnel.Active {
			address := tunnel.Data.Interface + ":" + tunnel.Data.Port
			listener, err := net.Listen("tcp", address)
			if err != nil {
				logs.Error("", "[WS-Agent] Failed to start SOCKS5 listener for agent %s: %v", agentId, err)
				return true
			}
			tunnel.listener = listener

			value, ok := ts.agents.Get(agentId)
			if !ok {
				logs.Error("", "[WS-Agent] Agent %s not found", agentId)
				_ = listener.Close()
				tunnel.listener = nil
				return true
			}
			agent, _ := value.(*Agent)

			go func() {
				for {
					var conn net.Conn
					conn, err = tunnel.listener.Accept()
					if err != nil {
						return
					}
					go handleTunChannelCreate(ts, agent, tunnel, conn)
				}
			}()

			tunnel.Active = true
			logs.Info("", "[WS-Agent] SOCKS5 listener started for agent %s on %s", agentId, address)

			// 更新任务状态
			taskData := adaptix.TaskData{
				TaskId:      tunnel.TaskId,
				Completed:   true,
				FinishDate:  time.Now().Unix(),
				Message:     fmt.Sprintf("WebSocket SOCKS5 tunnel established on %s", address),
				MessageType: CONSOLE_OUT_SUCCESS,
			}
			ts.TsTaskUpdate(agentId, taskData)
		}
		return true
	})
}

func (ts *Teamserver) TsTunnelWsAgentSessionClosed(agentId string) {
	var channelsToClose []struct {
		tunnel     *Tunnel
		tunChannel *TunnelChannel
	}

	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel, ok := value.(*Tunnel)
		if !ok {
			return true
		}
		if tunnel.Type != TUNNEL_WS_SOCKS5 || tunnel.Data.AgentId != agentId {
			return true
		}

		// 停止 SOCKS5 监听器
		if tunnel.listener != nil {
			logs.Info("", "[WS-Agent] Stopping SOCKS5 listener for agent %s", agentId)
			_ = tunnel.listener.Close()
			tunnel.listener = nil
			tunnel.Active = false

			// 更新任务状态
			taskData := adaptix.TaskData{
				TaskId:      tunnel.TaskId,
				Completed:   false,
				Message:     "WebSocket connection closed, SOCKS5 tunnel stopped",
				MessageType: CONSOLE_OUT_ERROR,
			}
			ts.TsTaskUpdate(agentId, taskData)
		}

		tunnel.connections.ForEach(func(cKey string, cValue interface{}) bool {
			ch, ok := cValue.(*TunnelChannel)
			if !ok {
				return true
			}
			channelsToClose = append(channelsToClose, struct {
				tunnel     *Tunnel
				tunChannel *TunnelChannel
			}{tunnel: tunnel, tunChannel: ch})
			return true
		})
		return true
	})

	for _, item := range channelsToClose {
		ts.closeWsTunnelChannel(item.tunnel, item.tunChannel, false)
	}
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		if tunnel.Data.Client == "" {
			if tunChannel.conn != nil {
				relaySocketToTunnel(agent, tunnel, tunChannel, ioDirect)
			} else {
				logs.Debug("", "[ERROR] tunChannel.conn is nil in relaySocketToTunnel")
			}
		} else {
			if tunChannel.wsconn != nil {
				relayWebsocketToTunnel(agent, tunnel, tunChannel, ioDirect)
			} else {
				logs.Debug("", "[ERROR] tunChannel.wsconn is nil in relayWebsocketToTunnel")
			}
		}
	}
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		if tunnel.Data.Client == "" {
			if tunChannel.conn != nil {
				_ = tunChannel.conn.Close()
			}
		} else {
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}
		}

		tunChannel.pwTun.Close()
		tunChannel.prTun.Close()
		tunChannel.pwSrv.Close()
		tunChannel.prSrv.Close()

		tunnel.connections.Delete(strconv.Itoa(channelId))
	}
}

func (ts *Teamserver) TsTunnelConnectionAccept(tunnelId int, channelId int) {
	tunId := fmt.Sprintf("%08x", tunnelId)
	value, ok := ts.tunnels.Get(tunId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	if tunnel.Data.Client == "" {
		handlerReverseAccept(agent, tunnel, channelId)
	} else {
		// TODO: reverse proxy to client
	}
}

/// handlers

func handleTunChannelCreate(ts *Teamserver, agent *Agent, tunnel *Tunnel, conn net.Conn) {

	tunChannel := &TunnelChannel{
		channelId: int(rand.Uint32()),
		conn:      conn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case TUNNEL_SOCKS4:
		targetAddress, targetPort, err := proxy.CheckSocks4(conn)
		if err != nil {
			logs.Debug("", "[ERROR] Socks4 proxy error: ", err)
			return
		}
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)

	case TUNNEL_SOCKS5:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5(conn)
		if err != nil {
			logs.Debug("", "[ERROR] Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_SOCKS5_AUTH:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5Auth(conn, tunnel.Data.AuthUser, tunnel.Data.AuthPass)
		if err != nil {
			logs.Debug("", "Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_LPORTFWD:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, tunnel.Data.Fhost, tport)

	case TUNNEL_WS_SOCKS5:
		// 先初始化 wsChannelID，确保在存储前已经设置
		for tunChannel.wsChannelID == 0 {
			tunChannel.wsChannelID = rand.Uint32()
		}
		tunnel.connections.Put(strconv.Itoa(tunChannel.channelId), tunChannel)
		ts.handleWsSocks5Connection(agent, tunnel, tunChannel)
		return

	default:
		return
	}

	tunnelManageTask(agent, taskData)

	tunnel.connections.Put(strconv.Itoa(tunChannel.channelId), tunChannel)
}

func (ts *Teamserver) handleWsSocks5Connection(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel) {
	if agent == nil || tunChannel == nil || tunnel == nil {
		logs.Error("", "[WS-Agent] handleWsSocks5Connection: nil parameters")
		return
	}

	logs.Info("", "[WS-Agent] Starting SOCKS5 connection handling for agent %s", agent.Data.Id)

	// 解析 SOCKS5 握手但不发送响应（等待 Agent 确认）
	targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5WithoutResponse(tunChannel.conn)
	if err != nil {
		logs.Error("", "[WS-Agent] SOCKS5 handshake parsing failed: %v", err)
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, false)
		return
	}

	logs.Info("", "[WS-Agent] SOCKS5 request parsed: target=%s:%d command=%d", targetAddress, targetPort, socksCommand)

	if !ts.AdaptixServer.WsTunnelHasSession(agent.Data.Id) {
		logs.Error("", "[WS-Agent] Agent %s has no active WebSocket session. Please execute 'wstunnel start <url>' command on the agent first!", agent.Data.Id)
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, false)
		return
	}

	if socksCommand == 3 {
		logs.Error("", "[WS-Agent] UDP associate not supported for WebSocket tunnel")
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, true)
		return
	}

	// wsChannelID 已经在 handleTunChannelCreate 中生成
	if tunChannel.wsChannelID == 0 {
		logs.Error("", "[WS-Agent] wsChannelID is 0, this should not happen!")
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, false)
		return
	}

	if tunChannel.wsReady == nil {
		tunChannel.wsReady = make(chan struct{})
	}
	if tunChannel.wsClosed == nil {
		tunChannel.wsClosed = make(chan struct{})
	}

	target := fmt.Sprintf("%s:%d", targetAddress, targetPort)
	logs.Info("", "[WS-Agent] Sending target %s to agent %s (channel %d)", target, agent.Data.Id, tunChannel.wsChannelID)
	logs.Debug("", "[WS-Agent] Channel state: channelId=%d, wsChannelID=%d, wsReady=%v, wsClosed=%v",
		tunChannel.channelId, tunChannel.wsChannelID, tunChannel.wsReady != nil, tunChannel.wsClosed != nil)

	if err := ts.AdaptixServer.WsTunnelSendTarget(agent.Data.Id, tunChannel.wsChannelID, target); err != nil {
		logs.Error("", "[WS-Agent] Failed to send target to agent %s: %v", agent.Data.Id, err)
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, true)
		return
	}

	// 等待 Agent 连接确认（最多 10 秒）
	logs.Info("", "[WS-Agent] Waiting for Agent ACK (channel %d, timeout 10s)...", tunChannel.wsChannelID)
	select {
	case <-tunChannel.wsReady:
		logs.Info("", "[WS-Agent] Agent ACK received, sending SOCKS5 success response")
		if err := proxy.SendSocks5Response(tunChannel.conn, true); err != nil {
			logs.Error("", "[WS-Agent] Failed to send SOCKS5 success response: %v", err)
			ts.closeWsTunnelChannel(tunnel, tunChannel, true)
			return
		}
		logs.Info("", "[WS-Agent] Channel %d → %s established successfully", tunChannel.wsChannelID, target)
		go ts.wsTunnelClientToAgent(agent, tunnel, tunChannel)
	case <-time.After(10 * time.Second):
		logs.Error("", "[WS-Agent] Timeout waiting for Agent ACK (channel %d)", tunChannel.wsChannelID)
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		ts.closeWsTunnelChannel(tunnel, tunChannel, true)
		return
	case <-tunChannel.wsClosed:
		logs.Error("", "[WS-Agent] Channel closed before ACK (channel %d)", tunChannel.wsChannelID)
		_ = proxy.SendSocks5Response(tunChannel.conn, false)
		return
	}
}

func (tunChannel *TunnelChannel) wsEnsureControl() {
	if tunChannel.wsReady == nil {
		tunChannel.wsReady = make(chan struct{})
	}
	if tunChannel.wsClosed == nil {
		tunChannel.wsClosed = make(chan struct{})
	}
}

func (tunChannel *TunnelChannel) wsSignalReady() {
	tunChannel.wsEnsureControl()
	tunChannel.wsReadyOnce.Do(func() {
		close(tunChannel.wsReady)
	})
}

func (tunChannel *TunnelChannel) wsSignalClosed() {
	tunChannel.wsEnsureControl()
	tunChannel.wsClosedOnce.Do(func() {
		close(tunChannel.wsClosed)
	})
}

func (ts *Teamserver) closeWsTunnelChannel(tunnel *Tunnel, tunChannel *TunnelChannel, notifyAgent bool) {
	if tunChannel == nil {
		return
	}

	if tunnel != nil && notifyAgent && tunChannel.wsChannelID != 0 {
		_ = ts.AdaptixServer.WsTunnelCloseChannel(tunnel.Data.AgentId, tunChannel.wsChannelID)
	}

	tunChannel.wsSignalReady()
	tunChannel.wsSignalClosed()

	if tunChannel.conn != nil {
		_ = tunChannel.conn.Close()
	}
	tunChannel.conn = nil
	if tunChannel.pwSrv != nil {
		_ = tunChannel.pwSrv.Close()
	}
	if tunChannel.prSrv != nil {
		_ = tunChannel.prSrv.Close()
	}
	if tunChannel.pwTun != nil {
		_ = tunChannel.pwTun.Close()
	}
	if tunChannel.prTun != nil {
		_ = tunChannel.prTun.Close()
	}

	if tunnel != nil {
		tunnel.connections.Delete(strconv.Itoa(tunChannel.channelId))
	}
}

func (ts *Teamserver) findWsChannel(agentId string, channelID uint32) (*Tunnel, *TunnelChannel) {
	var (
		resultTunnel  *Tunnel
		resultChannel *TunnelChannel
	)

	logs.Debug("", "[WS-Agent] Searching for channel agent=%s channelID=%d", agentId, channelID)

	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel, ok := value.(*Tunnel)
		if !ok {
			return true
		}
		if tunnel.Type != TUNNEL_WS_SOCKS5 || tunnel.Data.AgentId != agentId {
			return true
		}
		tunnel.connections.ForEach(func(cKey string, cValue interface{}) bool {
			ch, ok := cValue.(*TunnelChannel)
			if !ok {
				return true
			}
			logs.Debug("", "[WS-Agent] Checking channel: channelId=%d, wsChannelID=%d, target=%d", ch.channelId, ch.wsChannelID, channelID)
			if ch.wsChannelID == channelID {
				resultTunnel = tunnel
				resultChannel = ch
				logs.Debug("", "[WS-Agent] Found matching channel: channelId=%d, wsChannelID=%d", ch.channelId, ch.wsChannelID)
				return false
			}
			return true
		})
		return resultChannel == nil
	})

	if resultChannel == nil {
		logs.Debug("", "[WS-Agent] Channel not found: agent=%s channelID=%d", agentId, channelID)
	}

	return resultTunnel, resultChannel
}

func (ts *Teamserver) wsTunnelClientToAgent(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel) {
	if tunChannel == nil || agent == nil || tunnel == nil {
		logs.Error("", "[WS-Agent] wsTunnelClientToAgent: nil parameters")
		return
	}

	logs.Info("", "[WS-Agent] Starting data relay: Client -> Agent (channel %d)", tunChannel.wsChannelID)

	buf := make([]byte, 0x8000)
	for {
		select {
		case <-tunChannel.wsClosed:
			logs.Info("", "[WS-Agent] Channel %d closed, stopping relay", tunChannel.wsChannelID)
			return
		default:
		}

		n, err := tunChannel.conn.Read(buf)
		if n > 0 {
			logs.Debug("", "[WS-Agent] Read %d bytes from client (channel %d), forwarding to agent", n, tunChannel.wsChannelID)
			if err := ts.AdaptixServer.WsTunnelSendData(agent.Data.Id, tunChannel.wsChannelID, buf[:n]); err != nil {
				logs.Error("", "[WS-Agent] Send data failed agent=%s channel=%d err=%v", agent.Data.Id, tunChannel.wsChannelID, err)
				ts.closeWsTunnelChannel(tunnel, tunChannel, true)
				return
			}
		}
		if err != nil {
			if !errors.Is(err, io.EOF) {
				logs.Debug("", "[WS-Agent] Client connection closed (agent=%s channel=%d): %v", agent.Data.Id, tunChannel.wsChannelID, err)
			}
			ts.closeWsTunnelChannel(tunnel, tunChannel, true)
			return
		}
	}
}

func handleTunChannelCreateClient(agent *Agent, tunnel *Tunnel, wsconn *websocket.Conn, channelId int, targetAddress string, targetPort int, protocol string) {
	tunChannel := &TunnelChannel{
		channelId: channelId,
		conn:      nil,
		wsconn:    wsconn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case TUNNEL_SOCKS4:
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)

	case TUNNEL_SOCKS5:
		if protocol == "udp" {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_SOCKS5_AUTH:
		if protocol == "udp" {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_LPORTFWD:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, tunnel.Data.Fhost, tport)

	default:
		return
	}

	tunnelManageTask(agent, taskData)

	tunnel.connections.Put(strconv.Itoa(tunChannel.channelId), tunChannel)
}

func handlerReverseAccept(agent *Agent, tunnel *Tunnel, channelId int) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		rawTaskData := tunnel.handlerClose(channelId)
		tunnelManageTask(agent, rawTaskData)
		return
	}

	tunChannel := &TunnelChannel{
		channelId: channelId,
		conn:      fwdConn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	tunnel.connections.Put(strconv.Itoa(channelId), tunChannel)

	relaySocketToTunnel(agent, tunnel, tunChannel, false)
}

/// process socket

func tunnelManageTask(agent *Agent, taskData adaptix.TaskData) {
	taskData.AgentId = agent.Data.Id
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}

	agent.HostedTunnelTasks.Push(taskData)
}

func relayPipeToTaskData(agent *Agent, channelId int, taskData adaptix.TaskData) {
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id

	taskTunnel := adaptix.TaskDataTunnel{
		ChannelId: channelId,
		Data:      taskData,
	}

	agent.HostedTunnelData.Push(taskTunnel)
}

func relaySocketToTunnel(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	var taskData adaptix.TaskData
	var wg sync.WaitGroup
	ctx, cancel := context.WithCancel(context.Background())
	finish := func() {
		cancel()
		taskData = tunnel.handlerClose(tunChannel.channelId)
		tunnelManageTask(agent, taskData)
	}

	wg.Add(1)
	go func() {
		defer wg.Done()
		if tunChannel.pwSrv == nil || tunChannel.conn == nil {
			logs.Debug("", "[ERROR relaySocketToTunnel] pwSrv or conn == nil — copy (pwSrv <- conn)")
			return
		}
		io.Copy(tunChannel.pwSrv, tunChannel.conn)
		_ = tunChannel.pwSrv.Close()
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		if tunChannel.prTun == nil || tunChannel.conn == nil {
			logs.Debug("", "[ERROR relaySocketToTunnel] prTun or conn == nil — copy (conn <- prTun)")
			return
		}
		io.Copy(tunChannel.conn, tunChannel.prTun)
		if tcp, ok := tunChannel.conn.(*net.TCPConn); ok {
			_ = tcp.CloseWrite()
		}
	}()

	if !direct {
		go func() {
			buf := make([]byte, 0x8000)
			for {
				select {
				case <-ctx.Done():
					return
				default:
					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							td = tunnel.handlerWriteUDP(tunChannel.channelId, buf[:n])
						} else {
							td = tunnel.handlerWriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, td)
					}
					if err != nil {
						wg.Wait()
						finish()
						return
					}
				}
			}
		}()
	}
}

func relayWebsocketToTunnel(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	var taskData adaptix.TaskData
	var wg sync.WaitGroup
	ctx, cancel := context.WithCancel(context.Background())
	finish := func() {
		cancel()
		_ = tunChannel.wsconn.Close()
		taskData = tunnel.handlerClose(tunChannel.channelId)
		tunnelManageTask(agent, taskData)
	}

	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			_, msg, err := tunChannel.wsconn.ReadMessage()
			if err != nil {
				break
			}
			if _, err := tunChannel.pwSrv.Write(msg); err != nil {
				break
			}
		}
		_ = tunChannel.pwSrv.Close()
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		buf := make([]byte, 0x8000)
		for {
			n, err := tunChannel.prTun.Read(buf)
			if n > 0 {
				if err := tunChannel.wsconn.WriteMessage(websocket.BinaryMessage, buf[:n]); err != nil {
					break
				}
			}
			if err != nil {
				break
			}
		}
	}()

	if !direct {
		go func() {
			buf := make([]byte, 0x8000)
			for {
				select {
				case <-ctx.Done():
					return
				default:
					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							td = tunnel.handlerWriteUDP(tunChannel.channelId, buf[:n])
						} else {
							td = tunnel.handlerWriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, td)
					}
					if err != nil {
						wg.Wait()
						finish()
						return
					}
				}
			}
		}()
	}
}
