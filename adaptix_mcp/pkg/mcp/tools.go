package mcp

// NOTE: Keep MCP tool exposure minimal. Prefer consolidated tools with action/type
// parameters instead of adding many specialized tools (e.g., list_*, manage_*).
// If a new capability fits an existing domain (control/list/agent/task/session/system),
// extend that tool's action/type instead of adding a new top-level tool.

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

func (s *MCPServer) registerTools() {
	// Listen (Intelligence/Feedback)
	s.tools["listen_intelligence"] = s.handleListenIntelligence
	s.tools["read_archived_chat"] = s.handleReadArchivedChat

	// Quick asset view
	s.tools["look_assets"] = s.handleLookAssets

	// Control (Interaction/Orchestration/Execution)
	s.tools["control"] = s.handleControl

	// Extensions
	s.tools["inspect_extensions"] = s.handleInspectExtensions

	// Consolidated tools
	s.tools["c2"] = s.handleC2
}

func (s *MCPServer) handleMsfRequest(method string, baseURL string, path string, token string, payload map[string]interface{}) (interface{}, error) {
	url := baseURL + path

	var body io.Reader
	if payload != nil {
		data, err := json.Marshal(payload)
		if err != nil {
			return nil, fmt.Errorf("failed to encode msf request: %w", err)
		}
		body = bytes.NewBuffer(data)
	}

	req, err := http.NewRequest(method, url, body)
	if err != nil {
		return nil, fmt.Errorf("failed to build msf request: %w", err)
	}
	if payload != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}

	client := &http.Client{Timeout: 15 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("msf request failed: %w", err)
	}
	defer resp.Body.Close()

	respBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("msf response read failed: %w", err)
	}

	var result map[string]interface{}
	if len(respBytes) > 0 {
		if err := json.Unmarshal(respBytes, &result); err != nil {
			result = map[string]interface{}{"raw": string(respBytes)}
		}
	}

	if resp.StatusCode >= http.StatusBadRequest {
		if result == nil {
			result = map[string]interface{}{}
		}
		result["status_code"] = resp.StatusCode
		return result, fmt.Errorf("msf request failed with status %d", resp.StatusCode)
	}

	if result == nil {
		result = map[string]interface{}{}
	}
	result["status_code"] = resp.StatusCode
	return result, nil
}

func (s *MCPServer) handleListAgents(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_agents", params)
}

func (s *MCPServer) handleAgent(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "info":
		return s.clientConnector.SendCommand("get_agent_info", params)
	case "clear_console":
		return s.clientConnector.SendCommand("clear_console", params)
	case "console_output":
		return s.clientConnector.SendCommand("get_console_output", params)
	case "execute":
		return s.clientConnector.SendCommand("execute_command", params)
	case "execute_wait":
		return s.handleExecuteAndWait(params)
	case "update_config":
		return s.clientConnector.SendCommand("update_agent_config", params)
	case "update_metadata":
		return s.clientConnector.SendCommand("update_agent_metadata", params)
	}
	return nil, fmt.Errorf("unknown agent action: %s", action)
}

func (s *MCPServer) handleTask(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "output":
		return s.clientConnector.SendCommand("get_task_output", params)
	case "delete":
		return s.clientConnector.SendCommand("delete_tasks", params)
	}
	return nil, fmt.Errorf("unknown task action: %s", action)
}

func (s *MCPServer) handleSession(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "list":
		return s.clientConnector.SendCommand("list_sessions", params)
	case "archive":
		return s.clientConnector.SendCommand("archive_session", params)
	case "read":
		return s.clientConnector.SendCommand("read_session", params)
	}
	return nil, fmt.Errorf("unknown session action: %s", action)
}

func (s *MCPServer) handleSystem(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "capabilities":
		return s.clientConnector.SendCommand("get_capabilities", params)
	case "version":
		return s.clientConnector.SendCommand("get_version", params)
	case "ping":
		return s.clientConnector.SendCommand("ping", params)
	case "snapshot":
		snapshot := map[string]interface{}{}
		errors := map[string]string{}
		fetch := func(key string, cmd string, cmdParams map[string]interface{}) {
			resp, err := s.clientConnector.SendCommand(cmd, cmdParams)
			if err != nil {
				errors[key] = err.Error()
				return
			}
			if resp == nil || resp.Data == nil {
				snapshot[key] = map[string]interface{}{}
				return
			}
			snapshot[key] = resp.Data
		}

		fetch("agents", "list_agents", nil)
		fetch("tasks", "list_tasks", nil)
		fetch("listeners", "list_listeners", nil)
		fetch("tunnels", "list_tunnels", nil)
		fetch("targets", "list_targets", nil)
		fetch("pivots", "list_pivots", nil)
		fetch("sessions", "list_sessions", nil)
		fetch("credentials", "list_collected_data", map[string]interface{}{"data_type": "credentials"})
		fetch("downloads", "list_collected_data", map[string]interface{}{"data_type": "downloads"})
		fetch("screenshots", "list_collected_data", map[string]interface{}{"data_type": "screenshots"})

		if len(errors) > 0 {
			snapshot["errors"] = errors
		}
		return snapshot, nil
	}
	return nil, fmt.Errorf("unknown system action: %s", action)
}

func (s *MCPServer) handleC2(params map[string]interface{}) (interface{}, error) {
	domain, _ := params["domain"].(string)
	switch domain {
	case "list":
		return s.handleList(params)
	case "agent":
		return s.handleAgent(params)
	case "task":
		return s.handleTask(params)
	case "session":
		return s.handleSession(params)
	case "system":
		return s.handleSystem(params)
	}
	return nil, fmt.Errorf("unknown c2 domain: %s", domain)
}

func (s *MCPServer) handleControl(params map[string]interface{}) (interface{}, error) {
	domain, _ := params["domain"].(string)
	if domain == "" {
		domain, _ = params["action"].(string)
	}

	switch domain {
	case "speak":
		return s.handleSpeakInteraction(params)
	case "write":
		return s.handleWriteOrchestration(params)
	case "operate":
		return s.handleOperateControl(params)
	case "god_view":
		return s.handleGodView(params)
	case "flash":
		return s.handleTacticalFlash(params)
	case "msf":
		return s.handleMsfControl(params)
	case "tactical":
		action, _ := params["action"].(string)
		switch action {
		case "get_library":
			return s.clientConnector.SendCommand("tactical_get_library", params)
		case "modify_workflow":
			return s.clientConnector.SendCommand("tactical_modify_workflow", params)
		case "execute_sequence":
			return s.clientConnector.SendCommand("tactical_execute_sequence", params)
		case "read_results":
			return s.clientConnector.SendCommand("tactical_read_results", params)
		case "modify_library":
			return s.clientConnector.SendCommand("tactical_modify_library", params)
		case "broadcast_suggestion":
			return s.clientConnector.SendCommand("tactical_broadcast_suggestion", params)
		case "chat_response":
			return s.clientConnector.SendCommand("tactical_chat_response", params)
		}
		return nil, fmt.Errorf("unknown tactical action: %s", action)
	}
	return nil, fmt.Errorf("unknown control domain: %s", domain)
}

func (s *MCPServer) handleMsfControl(params map[string]interface{}) (interface{}, error) {
	baseURL, _ := params["base_url"].(string)
	if baseURL == "" {
		return nil, fmt.Errorf("missing base_url for msf control")
	}

	baseURL = strings.TrimRight(baseURL, "/")
	action, _ := params["action"].(string)
	token, _ := params["token"].(string)
	data, _ := params["data"].(map[string]interface{})
	consoleID, _ := params["console_id"].(string)
	command, _ := params["command"].(string)

	switch action {
	case "controller_start":
		return s.handleMsfRequest(http.MethodPost, baseURL, "/api/msf/controller/start", token, data)
	case "controller_stop":
		return s.handleMsfRequest(http.MethodPost, baseURL, "/api/msf/controller/stop", token, nil)
	case "controller_status":
		return s.handleMsfRequest(http.MethodGet, baseURL, "/api/msf/controller/status", token, nil)
	case "rpc_connect":
		return s.handleMsfRequest(http.MethodPost, baseURL, "/api/msf/start", token, nil)
	case "rpc_disconnect":
		return s.handleMsfRequest(http.MethodPost, baseURL, "/api/msf/stop", token, nil)
	case "rpc_status":
		return s.handleMsfRequest(http.MethodGet, baseURL, "/api/msf/status", token, nil)
	case "console_create":
		return s.handleMsfRequest(http.MethodPost, baseURL, "/api/msf/console/create", token, nil)
	case "console_read":
		if consoleID == "" {
			return nil, fmt.Errorf("missing console_id for console_read")
		}
		return s.handleMsfRequest(http.MethodGet, baseURL, fmt.Sprintf("/api/msf/console/%s/read", consoleID), token, nil)
	case "console_write":
		if consoleID == "" {
			return nil, fmt.Errorf("missing console_id for console_write")
		}
		if command == "" {
			return nil, fmt.Errorf("missing command for console_write")
		}
		return s.handleMsfRequest(http.MethodPost, baseURL, fmt.Sprintf("/api/msf/console/%s/write", consoleID), token, map[string]interface{}{"command": command})
	case "console_destroy":
		if consoleID == "" {
			return nil, fmt.Errorf("missing console_id for console_destroy")
		}
		return s.handleMsfRequest(http.MethodPost, baseURL, fmt.Sprintf("/api/msf/console/%s/destroy", consoleID), token, nil)
	}

	return nil, fmt.Errorf("unknown msf action: %s", action)
}

func (s *MCPServer) handleList(params map[string]interface{}) (interface{}, error) {
	listType, _ := params["type"].(string)
	switch listType {
	case "agents":
		return s.clientConnector.SendCommand("list_agents", params)
	case "tasks":
		return s.clientConnector.SendCommand("list_tasks", params)
	case "listeners":
		return s.clientConnector.SendCommand("list_listeners", params)
	case "filedelivery":
		return s.clientConnector.SendCommand("list_filedelivery", params)
	case "tunnels":
		return s.clientConnector.SendCommand("list_tunnels", params)
	case "targets":
		return s.clientConnector.SendCommand("list_targets", params)
	case "pivots":
		return s.clientConnector.SendCommand("list_pivots", params)
	case "sessions":
		return s.clientConnector.SendCommand("list_sessions", params)
	case "credentials", "downloads", "screenshots":
		return s.clientConnector.SendCommand("list_collected_data", map[string]interface{}{"data_type": listType})
	case "collected_data":
		dataType, _ := params["data_type"].(string)
		if dataType == "" {
			return nil, fmt.Errorf("missing data_type for collected_data")
		}
		return s.clientConnector.SendCommand("list_collected_data", map[string]interface{}{"data_type": dataType})
	}
	return nil, fmt.Errorf("unknown list type: %s", listType)
}

func (s *MCPServer) getToolDefinitions() []map[string]interface{} {
	return []map[string]interface{}{
		{
			"name":        "inspect_extensions",
			"description": "Inspect installed C2 extensions (BOFs/Scripts). Returns categories, available commands, descriptions, and usage examples. This is the MCP-level help equivalent for discovering command usage.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"filter": map[string]interface{}{
						"type":        "string",
						"description": "Optional keyword to filter extensions or commands (e.g. 'browser', 'creds').",
					},
					"root_path": map[string]interface{}{
						"type":        "string",
						"description": "Optional absolute path to Extension-Kit directory. Defaults to internal workspace path if omitted.",
					},
				},
			},
		},
		{
			"name":        "look_assets",
			"description": "Quick asset view for common inventories. Use when you need a fast snapshot without multiple list calls.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"agents", "listeners", "targets", "tunnels", "pivots"},
						"description": "Asset category to return.",
					},
				},
				"required": []string{"type"},
			},
		},
		{
			"name":        "control",
			"description": "Unified control endpoint for interaction, orchestration, execution, tactical flows, and autonomy. Use domain + action to route (speak/write/operate/god_view/tactical/flash/msf).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"domain": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"speak", "write", "operate", "god_view", "tactical", "flash", "msf"},
						"description": "Control domain (msf supports console + start/stop/connect).",
					},
					"action": map[string]interface{}{
						"type":        "string",
						"description": "Domain-specific action (e.g. speak: broadcast/enter_chat/team_chat; operate: execute/tunnel/file/pty/listener/target; tactical: get_library/modify_workflow/execute_sequence/read_results/modify_library/broadcast_suggestion/chat_response; god_view: query_status/suggest_action/autonomous; flash: summary; msf: controller_start/controller_stop/controller_status/rpc_connect/rpc_disconnect/rpc_status/console_create/console_read/console_write/console_destroy). Note: operate target currently supports remove only.",
					},
					"agent_id":        map[string]interface{}{"type": "string", "description": "Target agent ID (operate/tactical/write)."},
					"command":         map[string]interface{}{"type": "string", "description": "Command to execute (operate)."},
					"data":            map[string]interface{}{"type": "object", "description": "Payload for write/operate/tactical actions."},
					"content":         map[string]interface{}{"type": "string", "description": "Message content (speak)."},
					"target_user":     map[string]interface{}{"type": "string", "description": "Optional @mention target (speak)."},
					"suggestion":      map[string]interface{}{"type": "string", "description": "Suggestion content (god_view)."},
					"reasoning":       map[string]interface{}{"type": "string", "description": "Reasoning for suggestion (god_view)."},
					"enabled":         map[string]interface{}{"type": "boolean", "description": "Enable/disable autonomous (god_view)."},
					"summary":         map[string]interface{}{"type": "string", "description": "Flash summary (flash)."},
					"timeout_seconds": map[string]interface{}{"type": "number", "description": "Flash wait timeout (flash)."},
					"last_timestamp":  map[string]interface{}{"type": "number", "description": "Flash last timestamp (flash)."},
					"base_url":        map[string]interface{}{"type": "string", "description": "MSF API base URL (e.g. https://127.0.0.1:4321)."},
					"token":           map[string]interface{}{"type": "string", "description": "Access token for MSF API."},
					"console_id":      map[string]interface{}{"type": "string", "description": "MSF console ID (console_read/write/destroy)."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "c2",
			"description": "Unified C2 data/control plane. domain: list/agent/task/session/system. Use action/type fields based on domain.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"domain": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"list", "agent", "task", "session", "system"},
						"description": "C2 domain.",
					},
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"agents", "tasks", "listeners", "filedelivery", "tunnels", "targets", "pivots", "sessions", "credentials", "downloads", "screenshots", "collected_data"},
						"description": "List category (domain=list).",
					},
					"data_type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"credentials", "downloads", "screenshots"},
						"description": "Collected data type (domain=list, type=collected_data).",
					},
					"action": map[string]interface{}{
						"type":        "string",
						"description": "Action for agent/task/session/system domains. Note: task delete is not implemented in the client yet.",
					},
					"agent_id":      map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"command":       map[string]interface{}{"type": "string", "description": "Command to execute (agent execute)."},
					"sleep":         map[string]interface{}{"type": "number", "description": "Sleep interval in seconds."},
					"jitter":        map[string]interface{}{"type": "number", "description": "Jitter percentage."},
					"metadata_type": map[string]interface{}{"type": "string", "description": "Metadata field (tag/mark)."},
					"value":         map[string]interface{}{"type": "string", "description": "New metadata value."},
					"timeout":       map[string]interface{}{"type": "number", "description": "Wait timeout (execute_wait)."},
					"task_id":       map[string]interface{}{"type": "string", "description": "Task ID (task output)."},
					"task_ids":      map[string]interface{}{"type": "array", "items": map[string]interface{}{"type": "string"}, "description": "Task IDs (task delete)."},
					"session_id":    map[string]interface{}{"type": "string", "description": "Session ID (session read)."},
				},
				"required": []string{"domain"},
			},
		},
		{
			"name":        "listen_intelligence",
			"description": "Intelligence (Listen). Monitor outcomes. War Room modes: long-poll (timeout), latest-only (last_timestamp or max_messages=1), history (start_timestamp), or full log via resources/read. Use type='tasks'/'task_output' for operation results. Use type='console' when commands do not appear in tasks or when console-only output is expected.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"console", "tasks", "task_output", "collected_data", "chat"},
						"description": "Source. chat: monitor War Room; task_output: check a specific task result; console: session console output.",
					},
					"agent_id":         map[string]interface{}{"type": "string", "description": "Target agent ID (for console/tasks)."},
					"task_id":          map[string]interface{}{"type": "string", "description": "Task ID (for task_output)."},
					"timeout":          map[string]interface{}{"type": "number", "description": "Long-poll timeout (seconds) for chat. Use for blocking listen."},
					"last_timestamp":   map[string]interface{}{"type": "number", "description": "Latest-only mode: return messages newer than this timestamp."},
					"start_timestamp":  map[string]interface{}{"type": "number", "description": "History mode: fetch messages since this timestamp."},
					"max_messages":     map[string]interface{}{"type": "number", "description": "Limit returned chat messages (e.g. 1 for latest-only)."},
					"target_user":      map[string]interface{}{"type": "string", "description": "Filter chat by user."},
					"exclude_user":     map[string]interface{}{"type": "string", "description": "Exclude chat user."},
					"ignore_ai_prefix": map[string]interface{}{"type": "boolean", "description": "Ignore [Tactical AI] messages (default true).", "default": true},
					"data_type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"credentials", "downloads", "screenshots"},
						"description": "Category for collected_data.",
					},
				},
				"required": []string{"type"},
			},
		},
		{
			"name":        "read_archived_chat",
			"description": "Read archived War Room chat sessions after tactical_archive events. Useful for history browsing beyond the active log.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"limit": map[string]interface{}{"type": "number", "description": "Maximum archived sessions to return (default 5)."},
				},
			},
		},
	}
}

// --- Look Assets ---
func (s *MCPServer) handleLookAssets(params map[string]interface{}) (interface{}, error) {
	t, _ := params["type"].(string)
	switch t {
	case "agents":
		return s.clientConnector.SendCommand("list_agents", params)
	case "listeners":
		return s.clientConnector.SendCommand("list_listeners", params)
	case "targets":
		return s.clientConnector.SendCommand("list_targets", params)
	case "tunnels":
		return s.clientConnector.SendCommand("list_tunnels", params)
	case "pivots":
		return s.clientConnector.SendCommand("list_pivots", params)
	}
	return nil, fmt.Errorf("unknown asset type: %s", t)
}

// --- Listen Intelligence ---
func (s *MCPServer) handleListenIntelligence(params map[string]interface{}) (interface{}, error) {
	t, _ := params["type"].(string)
	switch t {
	case "console":
		return s.clientConnector.SendCommand("get_console_output", params)
	case "tasks":
		return s.clientConnector.SendCommand("list_tasks", params)
	case "task_output":
		return s.clientConnector.SendCommand("get_task_output", params)
	case "collected_data":
		return s.clientConnector.SendCommand("list_collected_data", params)
	case "chat":
		if !s.clientConnector.IsConnected() {
			if err := s.clientConnector.Connect(); err != nil {
				return nil, fmt.Errorf("failed to connect to Client MCP Bridge: %w", err)
			}
		}

		timeout := 300.0
		if val, ok := params["timeout"].(float64); ok && val > 0 {
			timeout = val
		}

		targetUser, _ := params["target_user"].(string)
		excludeUser, _ := params["exclude_user"].(string)

		ignoreAiPrefix := false
		if val, ok := params["ignore_ai_prefix"].(bool); ok {
			ignoreAiPrefix = val
		}

		isLikelyAiUser := func(username string) bool {
			name := strings.TrimSpace(strings.ToLower(username))
			return name == "ai" || name == "tactical ai" || name == "tactical_ai" || name == "tacticalai"
		}

		// Helper to check if message should be included
		shouldInclude := func(username string, content string) bool {
			if targetUser != "" && username != targetUser {
				return false
			}
			if excludeUser != "" && username == excludeUser {
				return false
			}
			if ignoreAiPrefix && strings.HasPrefix(content, "[Tactical AI]") {
				if username == "" || isLikelyAiUser(username) {
					return false
				}
			}
			return true
		}

		// Collect all unread messages
		lastRead, _ := params["last_timestamp"].(float64)
		startTimestamp, _ := params["start_timestamp"].(float64)
		maxMessages, _ := params["max_messages"].(float64)

		// If start_timestamp is provided and greater than last_timestamp, use it as the baseline
		if startTimestamp > lastRead {
			lastRead = startTimestamp
		}

		var unreadMsgs []map[string]interface{}
		for _, msg := range s.chatLog {
			ts, _ := msg["timestamp"].(int64)
			username, _ := msg["username"].(string)
			content, _ := msg["content"].(string)

			// Filter by timestamp and rules
			if float64(ts) > lastRead {
				if shouldInclude(username, content) {
					unreadMsgs = append(unreadMsgs, msg)
				}
			}
		}

		if len(unreadMsgs) > 0 {
			if maxMessages > 0 && len(unreadMsgs) > int(maxMessages) {
				unreadMsgs = unreadMsgs[len(unreadMsgs)-int(maxMessages):]
			}
			return map[string]interface{}{
				"status":   "received",
				"messages": unreadMsgs,
				"count":    len(unreadMsgs),
			}, nil
		}

		// If no unread, wait for new ones
		startTime := time.Now()
		for {
			elapsed := time.Since(startTime).Seconds()
			if elapsed >= timeout {
				break
			}

			select {
			case msg := <-s.chatChan:
				username, _ := msg["username"].(string)
				content, _ := msg["content"].(string)

				if shouldInclude(username, content) {
					return map[string]interface{}{
						"status":             "received",
						"messages":           []map[string]interface{}{msg},
						"count":              1,
						"system_instruction": "You are in AI Resident Mode. Analyze the messages. If action is needed, use tools. If not, wait. ALWAYS call adaptix mcp listen_intelligence again to keep monitoring. NOTE: Commands invalid for the target system (e.g. OS mismatch) may not appear in the task list.",
					}, nil
				}
				// If not the user we're looking for, keep waiting
			case <-time.After(1 * time.Second):
				// Just tick to re-check elapsed
			}
		}

		return map[string]interface{}{
			"status":  "timeout",
			"message": "No command received from operator within timeout",
		}, nil
	}
	return nil, fmt.Errorf("unknown intelligence type: %s", t)
}

func (s *MCPServer) handleGetAgentInfo(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_agent_info", params)
}

func (s *MCPServer) handleClearConsole(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("clear_console", params)
}

func (s *MCPServer) handleGetConsoleOutput(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_console_output", params)
}

func (s *MCPServer) handleExecuteCommand(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("execute_command", params)
}

func (s *MCPServer) handleUpdateAgentConfig(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("update_agent_config", params)
}

func (s *MCPServer) handleUpdateAgentMetadata(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("update_agent_metadata", params)
}

func (s *MCPServer) handleListTasks(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_tasks", params)
}

func (s *MCPServer) handleGetTaskOutput(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_task_output", params)
}

func (s *MCPServer) handleDeleteTasks(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("delete_tasks", params)
}

func (s *MCPServer) handleListListeners(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_listeners", params)
}

func (s *MCPServer) handleManageListener(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_listener", params)
}

func (s *MCPServer) handleListFileDelivery(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_filedelivery", params)
}

func (s *MCPServer) handleManageFileDelivery(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_filedelivery", params)
}

func (s *MCPServer) handleListTunnels(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_tunnels", params)
}

func (s *MCPServer) handleManageTunnel(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_tunnel", params)
}

func (s *MCPServer) handleListTargets(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_targets", params)
}

func (s *MCPServer) handleManageTarget(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_target", params)
}

func (s *MCPServer) handleListPivots(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_pivots", params)
}

func (s *MCPServer) handleListCollectedData(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_collected_data", params)
}

func (s *MCPServer) handleManagePty(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_pty", params)
}

func (s *MCPServer) handleGetCapabilities(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_capabilities", params)
}

func (s *MCPServer) handleGetVersion(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_version", params)
}

// --- Speak Interaction ---
func (s *MCPServer) handleSpeakInteraction(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "broadcast":
		return s.clientConnector.SendCommand("tactical_broadcast_suggestion", params)
	case "enter_chat":
		// This makes AI focus on tactical guidance
		library, err := s.clientConnector.SendCommand("tactical_get_library", nil)
		if err != nil {
			return nil, err
		}
		workflow, err := s.clientConnector.SendCommand("tactical_read_results", nil)
		if err != nil {
			return nil, err
		}

		result := map[string]interface{}{
			"status": "AI has entered tactical chat mode",
			"context": map[string]interface{}{
				"library":  library.Data,
				"workflow": workflow.Data,
			},
			"guidance": "You can now send guidance to the C2 operator, or orchestrate the next action based on the library context.",
		}
		return result, nil
	case "team_chat":
		content, _ := params["content"].(string)
		targetUser, _ := params["target_user"].(string)

		// Add AI identity prefix
		if !strings.HasPrefix(content, "[Tactical AI]") {
			content = "[Tactical AI] " + content
		}

		// Add @ target user if specified
		if targetUser != "" {
			content = "@" + targetUser + " " + content
		}

		params["content"] = content
		return s.clientConnector.SendCommand("send_team_chat", params)
	}
	return nil, fmt.Errorf("unknown interaction action: %s", action)
}

// --- Write Orchestration ---
func (s *MCPServer) handleWriteOrchestration(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	data, _ := params["data"].(map[string]interface{})

	switch action {
	case "modify_workflow":
		return s.clientConnector.SendCommand("tactical_modify_workflow", data)
	case "modify_library":
		return s.clientConnector.SendCommand("tactical_modify_library", data)
	case "update_agent_config":
		return s.clientConnector.SendCommand("update_agent_config", params)
	case "update_agent_metadata":
		return s.clientConnector.SendCommand("update_agent_metadata", params)
	}
	return nil, fmt.Errorf("unknown orchestration action: %s", action)
}

// --- Operate Control ---
func (s *MCPServer) handleOperateControl(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	data, _ := params["data"].(map[string]interface{})

	switch action {
	case "execute":
		return s.clientConnector.SendCommand("execute_command", params)
	case "tunnel":
		return s.clientConnector.SendCommand("manage_tunnel", data)
	case "file":
		return s.clientConnector.SendCommand("manage_filedelivery", data)
	case "pty":
		return s.clientConnector.SendCommand("manage_pty", data)
	case "listener":
		return s.clientConnector.SendCommand("manage_listener", data)
	case "target":
		return s.clientConnector.SendCommand("manage_target", data)
	}
	return nil, fmt.Errorf("unknown control action: %s", action)
}

// --- God View ---
func (s *MCPServer) handleGodView(params map[string]interface{}) (interface{}, error) {
	action, _ := params["action"].(string)
	switch action {
	case "query_status":
		return s.clientConnector.SendCommand("god_view_query_status", params)
	case "suggest_action":
		return s.clientConnector.SendCommand("god_view_suggest_action", params)
	case "autonomous":
		return s.clientConnector.SendCommand("ai_autonomous_control", params)
	}
	return nil, fmt.Errorf("unknown god_view action: %s", action)
}

// --- Tactical Flash (Mind Stone Style) ---
func (s *MCPServer) handleTacticalFlash(params map[string]interface{}) (interface{}, error) {
	summary, _ := params["summary"].(string)
	timeout := 600.0
	if val, ok := params["timeout_seconds"].(float64); ok && val > 0 {
		timeout = val
	}

	// 1. Broadcast the summary to the user first
	broadcastParams := map[string]interface{}{
		"action":  "broadcast",
		"content": summary,
	}
	_, err := s.handleSpeakInteraction(broadcastParams)
	if err != nil {
		utils.WarnLogger.Printf("Failed to broadcast flash summary: %v", err)
	}

	// 2. Use the chat listening logic to wait for feedback
	listenParams := map[string]interface{}{
		"type":           "chat",
		"timeout":        timeout,
		"last_timestamp": params["last_timestamp"],
	}

	return s.handleListenIntelligence(listenParams)
}

// --- Read Archived Chat ---
func (s *MCPServer) handleReadArchivedChat(params map[string]interface{}) (interface{}, error) {
	limit := 5
	if val, ok := params["limit"].(float64); ok && val > 0 {
		limit = int(val)
	}

	total := len(s.archivedLog)
	start := 0
	if total > limit {
		start = total - limit
	}

	archives := s.archivedLog[start:]

	return map[string]interface{}{
		"status":                  "success",
		"archives":                archives,
		"count":                   len(archives),
		"total_archived_sessions": total,
	}, nil
}

// --- Session Management ---

func (s *MCPServer) handleArchiveSession(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("archive_session", params)
}

func (s *MCPServer) handleListSessions(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_sessions", params)
}

func (s *MCPServer) handleReadSession(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("read_session", params)
}

// --- Legacy & Helpers ---
func (s *MCPServer) handleExecuteAndWait(params map[string]interface{}) (interface{}, error) {
	agentId, _ := params["agent_id"].(string)
	command, _ := params["command"].(string)
	timeout := 60.0
	if t, ok := params["timeout"].(float64); ok && t > 0 {
		timeout = t
	}

	// Step 1: Get current tasks to detect new task
	tasksResp, err := s.clientConnector.SendCommand("list_tasks", map[string]interface{}{"agent_id": agentId})
	if err != nil {
		return nil, fmt.Errorf("failed to get initial tasks: %v", err)
	}

	var existingTaskIds []string
	if tasksResp != nil && tasksResp.Data != nil {
		if tasks, ok := tasksResp.Data["tasks"].([]interface{}); ok {
			for _, t := range tasks {
				if task, ok := t.(map[string]interface{}); ok {
					if id, ok := task["task_id"].(string); ok {
						existingTaskIds = append(existingTaskIds, id)
					}
				}
			}
		}
	}

	// Step 2: Execute command
	_, err = s.clientConnector.SendCommand("execute_command", params)
	if err != nil {
		return nil, fmt.Errorf("failed to execute command: %v", err)
	}

	// Step 3: Wait for new task to appear and complete
	startTime := time.Now()
	pollInterval := 500 * time.Millisecond
	var newTaskId string

	for time.Since(startTime).Seconds() < timeout {
		time.Sleep(pollInterval)

		tasksResp, err := s.clientConnector.SendCommand("list_tasks", map[string]interface{}{"agent_id": agentId})
		if err != nil || tasksResp == nil || tasksResp.Data == nil {
			continue
		}

		tasks, ok := tasksResp.Data["tasks"].([]interface{})
		if !ok {
			continue
		}

		// Find new task
		for _, t := range tasks {
			task, ok := t.(map[string]interface{})
			if !ok {
				continue
			}
			taskId, _ := task["task_id"].(string)
			cmdLine, _ := task["command_line"].(string)

			// Check if this is our new task
			isNew := true
			for _, existingId := range existingTaskIds {
				if taskId == existingId {
					isNew = false
					break
				}
			}

			if isNew && cmdLine == command {
				newTaskId = taskId
				completed, _ := task["completed"].(bool)
				if completed {
					return map[string]interface{}{
						"task_id":      taskId,
						"agent_id":     agentId,
						"command_line": cmdLine,
						"status":       task["status"],
						"message":      task["message"],
						"output":       task["output"],
						"start_time":   task["start_time"],
						"finish_time":  task["finish_time"],
						"completed":    true,
					}, nil
				}
				break
			}
		}
	}

	if newTaskId != "" {
		return map[string]interface{}{
			"task_id":   newTaskId,
			"agent_id":  agentId,
			"command":   command,
			"status":    "timeout",
			"message":   fmt.Sprintf("Task not completed within %.0f seconds", timeout),
			"completed": false,
		}, nil
	}

	return map[string]interface{}{
		"agent_id":  agentId,
		"command":   command,
		"status":    "timeout",
		"message":   "Task not found within timeout",
		"completed": false,
	}, nil
}
