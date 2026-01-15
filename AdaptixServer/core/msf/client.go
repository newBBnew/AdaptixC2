package msf

import (
	"bytes"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"strings"
	"time"

	"AdaptixServer/core/utils/logs"

	"github.com/vmihailenco/msgpack/v5"
)

type MsfrpcClient struct {
	host   string
	port   int
	ssl    bool
	token  string
	client *http.Client
}

func NewClient(host string, port int, ssl bool) *MsfrpcClient {
	tr := &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		// 设置连接超时和保持连接
		IdleConnTimeout:       30 * time.Second,
		ResponseHeaderTimeout: 10 * time.Second,
		ExpectContinueTimeout: 1 * time.Second,
	}
	return &MsfrpcClient{
		host: host,
		port: port,
		ssl:  ssl,
		client: &http.Client{
			Timeout:   15 * time.Second, // 增加超时时间
			Transport: tr,
		},
	}
}

func (c *MsfrpcClient) URL(path string) string {
	scheme := "http"
	if c.ssl {
		scheme = "https"
	}
	return fmt.Sprintf("%s://%s:%d%s", scheme, c.host, c.port, path)
}

func (c *MsfrpcClient) Call(method string, args ...interface{}) (interface{}, error) {
	// 确保有token
	if c.token == "" && method != "auth.login" && method != "version" {
		return nil, fmt.Errorf("no authentication token")
	}

	// 构建正确的MessagePack格式：[method, token, ...args]
	var msg []interface{}
	if method == "auth.login" {
		// 这些方法不需要token
		msg = []interface{}{method}
		msg = append(msg, args...)
	} else {
		msg = []interface{}{method, c.token}
		msg = append(msg, args...)
	}

	data, err := msgpack.Marshal(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %v", err)
	}

	parts := strings.Split(method, ".")
	apiGroup := parts[0]

	// Log the MSF call
	logs.Debug("msf", "→ [RPC] POST %s", c.URL("/api/1.0/"+apiGroup))
	logs.Debug("msf", "→ [RPC] Method: %s, Args: %v", method, args)
	logs.Debug("msf", "→ [RPC] MsgPack size: %d bytes", len(data))

	req, err := http.NewRequest("POST", c.URL("/api/1.0/"+apiGroup), bytes.NewReader(data))
	if err != nil {
		logs.Error("msf", "← [RPC] Request create error: %v", err)
		return nil, fmt.Errorf("create request failed: %v", err)
	}

	req.Header.Set("Content-Type", "binary/message-pack")
	req.Header.Set("Accept", "binary/message-pack")

	// 重试逻辑：最多重试3次
	maxRetries := 3
	var resp *http.Response
	var reqErr error
	for i := 0; i < maxRetries; i++ {
		resp, reqErr = c.client.Do(req)
		if reqErr != nil {
			if i < maxRetries-1 {
				// 指数退避，等待一段时间后重试
				sleepDuration := time.Duration(200*(i+1)) * time.Millisecond
				logs.Warn("msf", "← [RPC] Retry %d/%d after error: %v", i+1, maxRetries, reqErr)
				time.Sleep(sleepDuration)
				continue
			}
			logs.Error("msf", "← [RPC] Failed after %d retries: %v", maxRetries, reqErr)
			return nil, fmt.Errorf("request failed after %d retries: %v", maxRetries, reqErr)
		}

		// 成功，跳出重试循环
		break
	}

	defer resp.Body.Close()

	respData, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		logs.Error("msf", "← [RPC] Read error: %v", err)
		return nil, fmt.Errorf("read response failed: %v", err)
	}

	logs.Debug("msf", "← [RPC] HTTP %d, Response size: %d bytes", resp.StatusCode, len(respData))

	// 检查HTTP状态
	if resp.StatusCode >= 400 {
		errorMsg := fmt.Sprintf("HTTP error %d: %s", resp.StatusCode, string(respData))
		logs.Error("msf", "← [RPC] HTTP Error: %s", errorMsg)

		// 尝试解析MSF错误消息
		if resp.StatusCode == 500 {
			if strings.Contains(string(respData), "Invalid Authentication Token") {
				errorMsg = "MSF authentication failed: invalid token"
			} else if strings.Contains(string(respData), "session") {
				errorMsg = "MSF session error: " + string(respData)
			}
		}

		return nil, fmt.Errorf("%s", errorMsg)
	}

	var result interface{}
	if err := msgpack.Unmarshal(respData, &result); err != nil {
		logs.Error("msf", "← [RPC] Decode error: %v", err)
		return nil, fmt.Errorf("decode failed: %v - raw data: %x", err, respData[:min(100, len(respData))])
	}

	// 检查MSF响应中的错误
	if m, ok := result.(map[string]interface{}); ok {
		if errMsg, ok := m["error"].(bool); ok && errMsg {
			if errorMsg, ok := m["error_message"].(string); ok {
				logs.Error("msf", "← [RPC] MSF Error: %s", errorMsg)
				return nil, fmt.Errorf("MSF error: %s", errorMsg)
			}
			logs.Error("msf", "← [RPC] MSF Error: %v", m)
			return nil, fmt.Errorf("MSF RPC error: %v", m)
		}
	}

	logs.Debug("msf", "← [RPC] Success: %s", method)
	return result, nil
}

// CheckConnection 检查MSF连接是否正常
func (c *MsfrpcClient) CheckConnection() error {
	if c.token == "" {
		return fmt.Errorf("not authenticated")
	}

	// 尝试获取版本信息来测试连接
	_, err := c.Version()
	if err != nil {
		return fmt.Errorf("connection test failed: %v", err)
	}

	return nil
}

func (c *MsfrpcClient) Login(user, pass string) (string, error) {
	// Log MSF login request
	logs.Debug("msf", "→ [LOGIN] POST %s", c.URL("/api/1.0/auth"))
	logs.Debug("msf", "→ [LOGIN] Content: [auth.login, %s, %s]", user, maskString(pass))
	logs.Debug("msf", "→ [LOGIN] Content-Type: binary/message-pack")

	// 修复：直接调用API，因为auth.login不需要token
	msg := []interface{}{"auth.login", user, pass}

	data, err := msgpack.Marshal(msg)
	if err != nil {
		return "", fmt.Errorf("encode failed: %v", err)
	}

	req, err := http.NewRequest("POST", c.URL("/api/1.0/auth"), bytes.NewReader(data))
	if err != nil {
		return "", fmt.Errorf("create request failed: %v", err)
	}

	req.Header.Set("Content-Type", "binary/message-pack")
	req.Header.Set("Accept", "binary/message-pack")

	resp, err := c.client.Do(req)
	if err != nil {
		logs.Error("msf", "← [LOGIN] Error: %v", err)
		return "", fmt.Errorf("request failed: %v", err)
	}
	defer resp.Body.Close()

	respData, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		logs.Error("msf", "← [LOGIN] Read error: %v", err)
		return "", fmt.Errorf("read response failed: %v", err)
	}

	logs.Debug("msf", "← [LOGIN] HTTP %d, Response size: %d bytes", resp.StatusCode, len(respData))

	var result map[string]interface{}
	if err := msgpack.Unmarshal(respData, &result); err != nil {
		logs.Error("msf", "← [LOGIN] Decode error: %v", err)
		return "", fmt.Errorf("decode failed: %v", err)
	}

	if errMsg, ok := result["error"].(bool); ok && errMsg {
		logs.Error("msf", "← [LOGIN] MSF error: %v", result)
		return "", fmt.Errorf("login failed: %v", result)
	}

	var token string
	if tokenStr, ok := result["token"].(string); ok {
		token = tokenStr
	} else if tokenBytes, ok := result["token"].([]byte); ok {
		token = string(tokenBytes)
	} else if tokenInterface, ok := result["token"]; ok {
		// 尝试转换为字符串
		switch v := tokenInterface.(type) {
		case string:
			token = v
		case []byte:
			token = string(v)
		default:
			return "", fmt.Errorf("invalid token type: %T", tokenInterface)
		}
	} else {
		return "", fmt.Errorf("no token in response: %v", result)
	}

	c.token = token
	logs.Debug("msf", "← [LOGIN] Success, token: %s...", maskString(token[:min(8, len(token))]))
	return token, nil
}

func (c *MsfrpcClient) Version() (string, error) {
	result, err := c.Call("core.version")
	if err != nil {
		return "", err
	}
	if m, ok := result.(map[string]interface{}); ok {
		if version, ok := m["version"].(string); ok {
			return version, nil
		}
	}
	return "", nil
}

func (c *MsfrpcClient) ConsoleCreate() (map[string]interface{}, error) {
	logs.Debug("msf", "→ [CONSOLE] console.create")
	result, err := c.Call("console.create")
	if err != nil {
		logs.Error("msf", "← [CONSOLE] Create failed: %v", err)
		return nil, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		if errMsg, ok := m["error"].(bool); ok && errMsg {
			logs.Error("msf", "← [CONSOLE] MSF error: %v", m)
			return nil, fmt.Errorf("console creation failed: %v", m)
		}
		if id, ok := m["id"].(string); ok {
			logs.Debug("msf", "← [CONSOLE] Created: id=%s", id)
		}
		return m, nil
	}
	return nil, fmt.Errorf("invalid console response: %v", result)
}

func (c *MsfrpcClient) ConsoleWrite(consoleID, data string) (int, error) {
	logs.Debug("msf", "→ [CONSOLE] console.write id=%s, data=%q", consoleID, truncate(data, 50))
	result, err := c.Call("console.write", consoleID, data)
	if err != nil {
		logs.Error("msf", "← [CONSOLE] Write failed: %v", err)
		return 0, err
	}

	if m, ok := result.(map[string]interface{}); ok {
		if errMsg, ok := m["error"].(bool); ok && errMsg {
			logs.Error("msf", "← [CONSOLE] MSF error: %v", m)
			return 0, fmt.Errorf("console write failed: %v", m)
		}
		if wrote, ok := m["wrote"].(int); ok {
			logs.Debug("msf", "← [CONSOLE] Wrote: %d bytes", wrote)
			return wrote, nil
		}
	}
	return 1, nil
}

func (c *MsfrpcClient) ConsoleRead(consoleID string) (map[string]interface{}, error) {
	logs.Debug("msf", "→ [CONSOLE] console.read id=%s", consoleID)
	result, err := c.Call("console.read", consoleID)
	if err != nil {
		logs.Error("msf", "← [CONSOLE] Read failed: %v", err)
		return nil, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		var data string
		if d, ok := m["data"].(string); ok {
			data = d
		} else if d, ok := m["data"].([]byte); ok {
			data = string(d)
		}
		busy := false
		if b, ok := m["busy"].(bool); ok {
			busy = b
		}
		// 将信息打包返回
		response := map[string]interface{}{
			"data": data,
			"busy": busy,
		}

		// 设置其他字段
		if prompt, ok := m["prompt"].(string); ok {
			response["prompt"] = prompt
		}

		logs.Debug("msf", "← [CONSOLE] Read: busy=%v, data_len=%d", busy, len(data))
		return response, nil
	}
	return nil, fmt.Errorf("invalid response: %v", result)
}

func (c *MsfrpcClient) ConsoleDestroy(consoleID string) error {
	_, err := c.Call("console.destroy", consoleID)
	return err
}

func (c *MsfrpcClient) SessionList() (map[string]SessionInfo, error) {
	result, err := c.Call("session.list")
	if err != nil {
		return nil, err
	}
	return ParseSessions(result), nil
}

func (c *MsfrpcClient) SessionShell(sessionID string) error {
	_, err := c.Call("session.shell", sessionID)
	return err
}

func (c *MsfrpcClient) SessionMeterpreter(sessionID string) error {
	_, err := c.Call("session.meterpreter", sessionID)
	return err
}

func (c *MsfrpcClient) SessionKill(sessionID string) error {
	_, err := c.Call("session.kill", sessionID)
	return err
}

func (c *MsfrpcClient) SessionWrite(sessionID string, data string) (int, error) {
	result, err := c.Call("session.write", sessionID, data)
	if err != nil {
		return 0, err
	}

	if m, ok := result.(map[string]interface{}); ok {
		if errMsg, ok := m["error"].(bool); ok && errMsg {
			return 0, fmt.Errorf("session write failed: %v", m)
		}
		if wrote, ok := m["wrote"].(int); ok {
			return wrote, nil
		}
	}

	return 0, nil
}

func (c *MsfrpcClient) SessionRead(sessionID string) (string, error) {
	result, err := c.Call("session.read", sessionID)
	if err != nil {
		return "", err
	}

	if m, ok := result.(map[string]interface{}); ok {
		if data, ok := m["data"].(string); ok {
			return data, nil
		}
	}

	return "", fmt.Errorf("invalid session read response")
}

func (c *MsfrpcClient) JobsList() (map[string]JobInfo, error) {
	result, err := c.Call("jobs.list")
	if err != nil {
		return nil, err
	}
	return ParseJobs(result), nil
}

func (c *MsfrpcClient) JobsInfo(jobID string) (JobInfo, error) {
	result, err := c.Call("jobs.info", jobID)
	if err != nil {
		return JobInfo{}, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		job := JobInfo{ID: jobID}
		if v, ok := m["name"].(string); ok {
			job.Name = v
		}
		if v, ok := m["status"].(string); ok {
			job.Status = v
		}
		return job, nil
	}
	return JobInfo{}, nil
}

func (c *MsfrpcClient) JobsKill(jobID string) error {
	_, err := c.Call("jobs.kill", jobID)
	return err
}

func (c *MsfrpcClient) ModulesList() ([]string, error) {
	result, err := c.Call("module.modules")
	if err != nil {
		return nil, err
	}
	if arr, ok := result.([]interface{}); ok {
		modules := make([]string, 0, len(arr))
		for _, m := range arr {
			if s, ok := m.(string); ok {
				modules = append(modules, s)
			}
		}
		return modules, nil
	}
	return nil, nil
}

func (c *MsfrpcClient) SearchModules(query string) ([]string, error) {
	result, err := c.Call("module.search", query)
	if err != nil {
		return nil, err
	}
	if arr, ok := result.([]interface{}); ok {
		modules := make([]string, 0, len(arr))
		for _, m := range arr {
			if s, ok := m.(string); ok {
				modules = append(modules, s)
			}
		}
		return modules, nil
	}
	return nil, nil
}

func (c *MsfrpcClient) ModuleInfo(moduleType, moduleName string) (map[string]interface{}, error) {
	result, err := c.Call("module.info", moduleType, moduleName)
	if err != nil {
		return nil, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		return m, nil
	}
	return nil, fmt.Errorf("invalid module.info response")
}

func (c *MsfrpcClient) ModuleOptions(moduleType, moduleName string) (map[string]interface{}, error) {
	result, err := c.Call("module.options", moduleType, moduleName)
	if err != nil {
		return nil, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		return m, nil
	}
	return nil, fmt.Errorf("invalid module.options response")
}

func (c *MsfrpcClient) ModuleCompatiblePayloads(moduleType, moduleName string) ([]string, error) {
	result, err := c.Call("module.compatible_payloads", moduleType, moduleName)
	if err != nil {
		return nil, err
	}
	if arr, ok := result.([]interface{}); ok {
		payloads := make([]string, 0, len(arr))
		for _, p := range arr {
			if s, ok := p.(string); ok {
				payloads = append(payloads, s)
			}
		}
		return payloads, nil
	}
	return nil, nil
}

func (c *MsfrpcClient) ModuleExecute(moduleType, moduleName string, options map[string]interface{}) (map[string]interface{}, error) {
	result, err := c.Call("module.execute", moduleType, moduleName, options)
	if err != nil {
		return nil, err
	}
	if m, ok := result.(map[string]interface{}); ok {
		return m, nil
	}
	return nil, fmt.Errorf("invalid module.execute response")
}

func (c *MsfrpcClient) IsConnected() bool {
	return c.token != ""
}

func (c *MsfrpcClient) GetToken() string {
	return c.token
}

func PrettyPrint(v interface{}) string {
	data, _ := json.MarshalIndent(v, "", "  ")
	return string(data)
}
