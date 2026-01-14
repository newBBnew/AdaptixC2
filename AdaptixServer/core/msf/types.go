package msf

import (
	"encoding/json"
	"fmt"
)

type MSFConfig struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	User     string `json:"user"`
	Password string `json:"password"`
	SSL      bool   `json:"ssl"`
}

type MSFStatus struct {
	Running   bool   `json:"running"`
	Connected bool   `json:"connected"`
	Message   string `json:"message"`
}

type ConsoleInfo struct {
	ID   string `json:"id"`
	Busy bool   `json:"busy"`
}

type ConsoleReadResponse struct {
	Data string `json:"data"`
	Busy bool   `json:"busy"`
}

type SessionInfo struct {
	ID          string `json:"id"`
	Type        string `json:"type"`
	Transport   string `json:"transport"`
	ViaExploit  string `json:"via_exploit"`
	ViaPayload  string `json:"via_payload"`
	Info        string `json:"info"`
	Workspace   string `json:"workspace"`
	SessionHost string `json:"session_host"`
	SessionPort int    `json:"session_port"`
	Username    string `json:"username"`
	UUID        string `json:"uuid"`
	Arch        string `json:"arch"`
	Platform    string `json:"platform"`
}

type JobInfo struct {
	ID        string `json:"id"`
	Name      string `json:"name"`
	Status    string `json:"status"`
	StartTime string `json:"start_time"`
}

type WSMessage struct {
	Type string      `json:"type"`
	Data interface{} `json:"data"`
}

type WSConsoleWrite struct {
	ConsoleID string `json:"console_id"`
	Command   string `json:"command"`
}

type WSSessionInteract struct {
	SessionID string `json:"session_id"`
	Command   string `json:"command"`
}

func (c *MSFConfig) ToEnv() map[string]string {
	sslStr := "false"
	if c.SSL {
		sslStr = "true"
	}
	return map[string]string{
		"MSF_HOST": c.Host,
		"MSF_PORT": fmt.Sprintf("%d", c.Port),
		"MSF_USER": c.User,
		"MSF_PASS": c.Password,
		"MSF_SSL":  sslStr,
	}
}

func (s *MSFStatus) ToJSON() string {
	data, _ := json.Marshal(s)
	return string(data)
}

func ParseSessions(raw interface{}) map[string]SessionInfo {
	result := make(map[string]SessionInfo)
	if rawMap, ok := raw.(map[string]interface{}); ok {
		for id, val := range rawMap {
			if sessionMap, ok := val.(map[string]interface{}); ok {
				session := SessionInfo{ID: id}
				if v, ok := sessionMap["type"].(string); ok {
					session.Type = v
				}
				if v, ok := sessionMap["transport"].(string); ok {
					session.Transport = v
				}
				if v, ok := sessionMap["via_exploit"].(string); ok {
					session.ViaExploit = v
				}
				if v, ok := sessionMap["via_payload"].(string); ok {
					session.ViaPayload = v
				}
				if v, ok := sessionMap["info"].(string); ok {
					session.Info = v
				}
				if v, ok := sessionMap["workspace"].(string); ok {
					session.Workspace = v
				}
				if v, ok := sessionMap["session_host"].(string); ok {
					session.SessionHost = v
				}
				if v, ok := sessionMap["session_port"].(float64); ok {
					session.SessionPort = int(v)
				}
				if v, ok := sessionMap["username"].(string); ok {
					session.Username = v
				}
				if v, ok := sessionMap["uuid"].(string); ok {
					session.UUID = v
				}
				if v, ok := sessionMap["arch"].(string); ok {
					session.Arch = v
				}
				if v, ok := sessionMap["platform"].(string); ok {
					session.Platform = v
				}
				result[id] = session
			}
		}
	}
	return result
}

func ParseJobs(raw interface{}) map[string]JobInfo {
	result := make(map[string]JobInfo)
	if rawMap, ok := raw.(map[string]interface{}); ok {
		for id, val := range rawMap {
			if jobMap, ok := val.(map[string]interface{}); ok {
				job := JobInfo{ID: id}
				if v, ok := jobMap["name"].(string); ok {
					job.Name = v
				}
				if v, ok := jobMap["status"].(string); ok {
					job.Status = v
				}
				if v, ok := jobMap["start_time"].(string); ok {
					job.StartTime = v
				}
				result[id] = job
			}
		}
	}
	return result
}
