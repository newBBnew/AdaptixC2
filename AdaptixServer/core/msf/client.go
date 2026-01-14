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
	}
	return &MsfrpcClient{
		host: host,
		port: port,
		ssl:  ssl,
		client: &http.Client{
			Timeout:   30 * time.Second,
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
	msg := []interface{}{method}
	msg = append(msg, args...)

	data, err := msgpack.Marshal(msg)
	if err != nil {
		return nil, fmt.Errorf("encode failed: %v", err)
	}

	parts := strings.Split(method, ".")
	apiGroup := parts[0]

	if method == "version" {
		apiGroup = "version"
	}

	req, err := http.NewRequest("POST", c.URL("/api/1.0/"+apiGroup), bytes.NewReader(data))
	if err != nil {
		return nil, fmt.Errorf("create request failed: %v", err)
	}

	req.Header.Set("Content-Type", "binary/message-pack")
	req.Header.Set("Accept", "binary/message-pack")

	resp, err := c.client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %v", err)
	}
	defer resp.Body.Close()

	respData, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response failed: %v", err)
	}

	var result interface{}
	if err := msgpack.Unmarshal(respData, &result); err != nil {
		return nil, fmt.Errorf("decode failed: %v", err)
	}

	return result, nil
}

func (c *MsfrpcClient) Login(user, pass string) (string, error) {
	result, err := c.Call("auth.login", user, pass)
	if err != nil {
		return "", err
	}

	if m, ok := result.(map[string]interface{}); ok {
		if errMsg, ok := m["error"].(bool); ok && errMsg {
			return "", fmt.Errorf("login failed: %v", m)
		}
		var token string
		if tokenStr, ok := m["token"].(string); ok {
			token = tokenStr
		} else if tokenBytes, ok := m["token"].([]byte); ok {
			token = string(tokenBytes)
		} else {
			return "", fmt.Errorf("no token in response: %v", m)
		}
		c.token = token
		return token, nil
	}

	return "", fmt.Errorf("invalid response format: %v", result)
}

func (c *MsfrpcClient) Version() (string, error) {
	result, err := c.Call("version")
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

func (c *MsfrpcClient) ConsoleCreate() (string, error) {
	result, err := c.Call("console.create", c.token)
	if err != nil {
		return "", err
	}
	if m, ok := result.(map[string]interface{}); ok {
		if id, ok := m["id"].(string); ok {
			return id, nil
		}
		return "", fmt.Errorf("no console id: %v", m)
	}
	return "", fmt.Errorf("invalid response: %v", result)
}

func (c *MsfrpcClient) ConsoleWrite(consoleID, data string) error {
	_, err := c.Call("console.write", c.token, consoleID, data)
	return err
}

func (c *MsfrpcClient) ConsoleRead(consoleID string) (string, bool, error) {
	result, err := c.Call("console.read", c.token, consoleID)
	if err != nil {
		return "", false, err
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
		return data, busy, nil
	}
	return "", false, fmt.Errorf("invalid response: %v", result)
}

func (c *MsfrpcClient) ConsoleDestroy(consoleID string) error {
	_, err := c.Call("console.destroy", c.token, consoleID)
	return err
}

func (c *MsfrpcClient) SessionList() (map[string]SessionInfo, error) {
	result, err := c.Call("session.list", c.token)
	if err != nil {
		return nil, err
	}
	return ParseSessions(result), nil
}

func (c *MsfrpcClient) SessionShell(sessionID string) error {
	_, err := c.Call("session.shell", c.token, sessionID)
	return err
}

func (c *MsfrpcClient) SessionMeterpreter(sessionID string) error {
	_, err := c.Call("session.meterpreter", c.token, sessionID)
	return err
}

func (c *MsfrpcClient) SessionKill(sessionID string) error {
	_, err := c.Call("session.kill", c.token, sessionID)
	return err
}

func (c *MsfrpcClient) JobsList() (map[string]JobInfo, error) {
	result, err := c.Call("jobs.list", c.token)
	if err != nil {
		return nil, err
	}
	return ParseJobs(result), nil
}

func (c *MsfrpcClient) JobsInfo(jobID string) (JobInfo, error) {
	result, err := c.Call("jobs.info", c.token, jobID)
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
	_, err := c.Call("jobs.kill", c.token, jobID)
	return err
}

func (c *MsfrpcClient) ModulesList() ([]string, error) {
	result, err := c.Call("module.modules", c.token)
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
	result, err := c.Call("module.search", c.token, query)
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
