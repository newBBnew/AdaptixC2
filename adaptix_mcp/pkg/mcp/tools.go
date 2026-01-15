package mcp

import (
	"fmt"
	"strings"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

func (s *MCPServer) registerTools() {
	// Look (Assets/Situational Awareness)
	s.tools["look_assets"] = s.handleLookAssets

	// Listen (Intelligence/Feedback)
	s.tools["listen_intelligence"] = s.handleListenIntelligence

	// Speak (Interaction/Guidance)
	s.tools["speak_interaction"] = s.handleSpeakInteraction

	// Write (Orchestration/Configuration)
	s.tools["write_orchestration"] = s.handleWriteOrchestration

	// Operate (Action/Execution)
	s.tools["operate_control"] = s.handleOperateControl

	// God View (Global Awareness & Autonomy)
	s.tools["god_view"] = s.handleGodView

	// Tactical (Decision/Interaction)
	s.tools["tactical_flash"] = s.handleTacticalFlash

	// Session Management
	s.tools["archive_session"] = s.handleArchiveSession
	s.tools["list_sessions"] = s.handleListSessions
	s.tools["read_session"] = s.handleReadSession

	// Extensions
	s.tools["inspect_extensions"] = s.handleInspectExtensions

	// Agent/Console
	s.tools["list_agents"] = s.handleListAgents
	s.tools["get_agent_info"] = s.handleGetAgentInfo
	s.tools["clear_console"] = s.handleClearConsole
	s.tools["get_console_output"] = s.handleGetConsoleOutput
	s.tools["execute_command"] = s.handleExecuteCommand
	s.tools["update_agent_config"] = s.handleUpdateAgentConfig
	s.tools["update_agent_metadata"] = s.handleUpdateAgentMetadata

	// Tasks
	s.tools["list_tasks"] = s.handleListTasks
	s.tools["get_task_output"] = s.handleGetTaskOutput
	s.tools["delete_tasks"] = s.handleDeleteTasks

	// Listeners
	s.tools["list_listeners"] = s.handleListListeners
	s.tools["manage_listener"] = s.handleManageListener

	// File Delivery
	s.tools["list_filedelivery"] = s.handleListFileDelivery
	s.tools["manage_filedelivery"] = s.handleManageFileDelivery

	// Tunnels
	s.tools["list_tunnels"] = s.handleListTunnels
	s.tools["manage_tunnel"] = s.handleManageTunnel

	// Targets & Pivots
	s.tools["list_targets"] = s.handleListTargets
	s.tools["manage_target"] = s.handleManageTarget
	s.tools["list_pivots"] = s.handleListPivots

	// Collected Data
	s.tools["list_collected_data"] = s.handleListCollectedData

	// PTY
	s.tools["manage_pty"] = s.handleManagePty

	// Introspection
	s.tools["get_capabilities"] = s.handleGetCapabilities
	s.tools["get_version"] = s.handleGetVersion

	// Legacy support (optional, but good for transition)
	s.tools["execute_and_wait"] = s.handleExecuteAndWait
}

func (s *MCPServer) handleListAgents(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_agents", params)
}

func (s *MCPServer) getToolDefinitions() []map[string]interface{} {
	return []map[string]interface{}{
		{
			"name":        "inspect_extensions",
			"description": "Inspect installed C2 extensions (BOFs/Scripts). Returns categories, available commands, descriptions, and usage examples. Use this to discover capabilities.",
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
			"name":        "list_agents",
			"description": "List all active agents.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "get_agent_info",
			"description": "Fetch detailed agent information by agent_id.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			"name":        "clear_console",
			"description": "Clear agent console output.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			"name":        "get_console_output",
			"description": "Fetch full console output for an agent.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			"name":        "execute_command",
			"description": "Execute a command on an agent console.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"command":  map[string]interface{}{"type": "string", "description": "Command to execute."},
				},
				"required": []string{"agent_id", "command"},
			},
		},
		{
			"name":        "update_agent_config",
			"description": "Update agent runtime config (sleep/jitter).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"sleep":    map[string]interface{}{"type": "number", "description": "Sleep interval in seconds."},
					"jitter":   map[string]interface{}{"type": "number", "description": "Jitter percentage."},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			"name":        "update_agent_metadata",
			"description": "Update agent metadata (tag/mark).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id":      map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"metadata_type": map[string]interface{}{"type": "string", "description": "Metadata field (tag/mark)."},
					"value":         map[string]interface{}{"type": "string", "description": "New value."},
				},
				"required": []string{"agent_id", "metadata_type", "value"},
			},
		},
		{
			"name":        "list_tasks",
			"description": "List tasks (optionally filter by agent).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Optional agent ID filter."},
				},
			},
		},
		{
			"name":        "get_task_output",
			"description": "Fetch detailed output for a task.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"task_id": map[string]interface{}{"type": "string", "description": "Task ID."},
				},
				"required": []string{"task_id"},
			},
		},
		{
			"name":        "delete_tasks",
			"description": "Delete tasks (server support may vary).",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "list_listeners",
			"description": "List active listeners.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "manage_listener",
			"description": "Manage listeners (start/stop/edit).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{"type": "string", "description": "Action (start/stop/edit)."},
					"name":   map[string]interface{}{"type": "string", "description": "Listener name."},
					"type":   map[string]interface{}{"type": "string", "description": "Listener type (start/edit)."},
					"data":   map[string]interface{}{"type": "string", "description": "Listener config JSON string."},
				},
				"required": []string{"action", "name"},
			},
		},
		{
			"name":        "list_filedelivery",
			"description": "List hosted files available for delivery.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "manage_filedelivery",
			"description": "Manage hosted files (upload, delete, create_link).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"upload", "delete", "create_link"},
						"description": "Action type.",
					},
					"local_path":   map[string]interface{}{"type": "string", "description": "Local path for upload."},
					"file_name":    map[string]interface{}{"type": "string", "description": "Optional filename override for upload."},
					"file_id":      map[string]interface{}{"type": "string", "description": "File ID for delete/create_link."},
					"expire_hours": map[string]interface{}{"type": "number", "description": "Link expiry hours (create_link)."},
					"max_uses":     map[string]interface{}{"type": "number", "description": "Max uses (create_link)."},
					"allowed_ip":   map[string]interface{}{"type": "string", "description": "Allowed IP (create_link)."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "list_pivots",
			"description": "List pivots.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "list_collected_data",
			"description": "List collected data (credentials/downloads/screenshots).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"data_type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"credentials", "downloads", "screenshots"},
						"description": "Collected data type.",
					},
				},
				"required": []string{"data_type"},
			},
		},
		{
			"name":        "list_tunnels",
			"description": "List active tunnels.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "manage_tunnel",
			"description": "Manage tunnels (start/stop/edit).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action":    map[string]interface{}{"type": "string", "description": "Action (start/stop/edit)."},
					"type":      map[string]interface{}{"type": "string", "description": "Tunnel type (start)."},
					"tunnel_id": map[string]interface{}{"type": "string", "description": "Tunnel ID (stop/edit)."},
					"info":      map[string]interface{}{"type": "string", "description": "Tunnel info (edit)."},
					"data":      map[string]interface{}{"type": "object", "description": "Tunnel config (start)."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "list_targets",
			"description": "List discovered targets.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "manage_target",
			"description": "Manage discovered targets (remove).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"remove"},
						"description": "Action type.",
					},
					"target_id":  map[string]interface{}{"type": "string", "description": "Single target ID."},
					"target_ids": map[string]interface{}{"type": "array", "items": map[string]interface{}{"type": "string"}, "description": "List of target IDs."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "manage_pty",
			"description": "Manage PTY sessions (open, read, write, close, list).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"open", "read", "write", "close", "list"},
						"description": "Action type.",
					},
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"pty_id":   map[string]interface{}{"type": "string", "description": "PTY session ID."},
					"program":  map[string]interface{}{"type": "string", "description": "Program for open."},
					"rows":     map[string]interface{}{"type": "number", "description": "Rows for open."},
					"cols":     map[string]interface{}{"type": "number", "description": "Cols for open."},
					"data":     map[string]interface{}{"type": "string", "description": "Data to write."},
					"base64":   map[string]interface{}{"type": "boolean", "description": "Whether data is base64."},
					"clear":    map[string]interface{}{"type": "boolean", "description": "Clear buffer on read."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "get_capabilities",
			"description": "List MCP bridge capabilities exposed by the client.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "get_version",
			"description": "Return MCP protocol and framework version.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "look_assets",
			"description": "Reconnaissance (Look). View the battlefield: agents (C2 channels), listeners, targets, tunnels, and pivots. Use this to orient yourself before acting.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"agents", "listeners", "targets", "tunnels", "pivots"},
						"description": "Asset category.",
					},
					"filter": map[string]interface{}{
						"type":        "string",
						"description": "Optional filter keyword (e.g. agent ID).",
					},
				},
				"required": []string{"type"},
			},
		},
		{
			"name":        "listen_intelligence",
			"description": "Intelligence (Listen). Monitor outcomes. Use type='chat' to wait for instructions in the War Room. Use type='tasks'/'task_output' to check specific operation results. Avoid 'console' unless auditing.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"console", "tasks", "task_output", "collected_data", "chat"},
						"description": "Source. chat: monitor War Room; task_output: check specific task result.",
					},
					"agent_id":         map[string]interface{}{"type": "string", "description": "Target agent ID (for console/tasks)."},
					"task_id":          map[string]interface{}{"type": "string", "description": "Task ID (for task_output)."},
					"timeout":          map[string]interface{}{"type": "number", "description": "Wait timeout for chat (default 300s)."},
					"last_timestamp":   map[string]interface{}{"type": "number", "description": "Last read timestamp for chat (for polling)."},
					"start_timestamp":  map[string]interface{}{"type": "number", "description": "Filter messages older than this timestamp."},
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
			"name":        "speak_interaction",
			"description": "Interaction (Speak). Communicate with operators in the War Room. Use 'team_chat' to send updates or ask for authorization. Use 'broadcast' for high-priority tactical alerts.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"broadcast", "enter_chat", "team_chat"},
						"description": "Action type. team_chat: standard communication; broadcast: priority alert; enter_chat: signal readiness.",
					},
					"content": map[string]interface{}{
						"type":        "string",
						"description": "Message content.",
					},
					"target_user": map[string]interface{}{
						"type":        "string",
						"description": "Optional @mention target.",
					},
					"task_id": map[string]interface{}{
						"type":        "string",
						"description": "Optional related task ID.",
					},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "write_orchestration",
			"description": "Orchestration (Write). Modify tactical library, workflow plans, or agent configurations.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"modify_workflow", "modify_library", "update_agent_config", "update_agent_metadata"},
						"description": "Action type.",
					},
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"data":     map[string]interface{}{"type": "object", "description": "Configuration data."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "operate_control",
			"description": "Execution (Operate). Perform actions: execute commands, manage tunnels, file delivery, PTYs, listeners.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"execute", "tunnel", "file", "pty", "listener"},
						"description": "Operation type.",
					},
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"command":  map[string]interface{}{"type": "string", "description": "Command to execute."},
					"data":     map[string]interface{}{"type": "object", "description": "Operation parameters."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "god_view",
			"description": "Global awareness and autonomy (God View). query_status: fetch full C2 status (agents, tasks, targets, listeners); suggest_action: send tactical suggestions with reasoning; autonomous: enable/disable autonomous mode.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"query_status", "suggest_action", "autonomous"},
						"description": "query_status: fetch full status; suggest_action: send a suggestion; autonomous: set autonomous mode.",
					},
					"suggestion": map[string]interface{}{"type": "string", "description": "Suggestion content for suggest_action."},
					"reasoning":  map[string]interface{}{"type": "string", "description": "Reasoning for suggest_action."},
					"enabled":    map[string]interface{}{"type": "boolean", "description": "Enable/disable flag for autonomous."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "execute_and_wait",
			"description": "Execute and fetch output atomically. This is a convenience wrapper around operate_control(execute) + listen_intelligence(task_output). Recommended for quick commands expected to finish within ~60 seconds (e.g., whoami, netstat, tasklist).",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"command":  map[string]interface{}{"type": "string", "description": "Command to execute."},
					"timeout":  map[string]interface{}{"type": "number", "description": "Maximum wait time in seconds."},
				},
				"required": []string{"agent_id", "command"},
			},
		},
		{
			"name":        "get_capabilities",
			"description": "List MCP bridge capabilities exposed by the client.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "get_version",
			"description": "Return MCP protocol and framework version.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "tactical_flash",
			"description": "Advanced collaboration tool (Mind Stone style). The AI initiates a tactical suggestion or question and blocks while waiting for operator feedback, forming a clear think -> suggest -> confirm loop. If there is no response, it times out and the AI may call it again as needed.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"summary": map[string]interface{}{
						"type":        "string",
						"description": "Summary of the tactical suggestion or question to send to the operator.",
					},
					"timeout_seconds": map[string]interface{}{
						"type":        "number",
						"description": "Maximum wait time in seconds (default 600s).",
					},
					"last_timestamp": map[string]interface{}{
						"type":        "number",
						"description": "Timestamp of last read message; used to check for immediate feedback.",
					},
				},
				"required": []string{"summary"},
			},
		},
		{
			"name":        "archive_session",
			"description": "Archive the current active chat session. This clears the 'War Room' and moves all messages to a historical session. Use this when the current context becomes too large or when starting a new distinct task.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "list_sessions",
			"description": "List all archived chat sessions.",
			"inputSchema": map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			"name":        "read_session",
			"description": "Read the content of a specific archived session.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"session_id": map[string]interface{}{"type": "string", "description": "ID of the session to read."},
				},
				"required": []string{"session_id"},
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
		timeout := 300.0
		if val, ok := params["timeout"].(float64); ok && val > 0 {
			timeout = val
		}

		targetUser, _ := params["target_user"].(string)
		excludeUser, _ := params["exclude_user"].(string)

		ignoreAiPrefix := true
		if val, ok := params["ignore_ai_prefix"].(bool); ok {
			ignoreAiPrefix = val
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
				return false
			}
			return true
		}

		// Collect all unread messages
		lastRead, _ := params["last_timestamp"].(float64)
		startTimestamp, _ := params["start_timestamp"].(float64)

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
						"system_instruction": "You are in AI Resident Mode. Analyze the messages. If action is needed, use tools. If not, wait. ALWAYS call listen_intelligence again to keep monitoring. NOTE: Commands invalid for the target system (e.g. OS mismatch) may not appear in the task list.",
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
