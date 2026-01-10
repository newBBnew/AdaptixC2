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

	// Legacy support (optional, but good for transition)
	s.tools["execute_and_wait"] = s.handleExecuteAndWait
}

func (s *MCPServer) getToolDefinitions() []map[string]interface{} {
	return []map[string]interface{}{
		{
			"name":        "look_assets",
			"description": "Reconnaissance and asset discovery (Look). Use this before taking actions. The 'agents' category represents active communication channels between the C2 and controlled hosts. Use this tool to obtain a full situational picture (agent channels, listeners, target hosts, tunnels). Call it first to establish the current operational boundary.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"agents", "listeners", "targets", "tunnels", "pivots"},
						"description": "Asset category. agents: list active C2-to-agent channels; listeners: view egress boundary; targets: discovered hosts; tunnels: current forwarding paths.",
					},
					"filter": map[string]interface{}{
						"type":        "string",
						"description": "Optional filter keyword to narrow results (e.g., an agent ID).",
					},
				},
				"required": []string{"type"},
			},
		},
		{
			"name":        "listen_intelligence",
			"description": "Intelligence and feedback loop (Listen). Use this to accurately track outcomes. Rule: do not blindly read type='console' because it may contain large, unordered output that quickly exhausts context. Prefer type='tasks' to locate the relevant task_id, then use type='task_output' with that task_id to fetch the exact result for a single task. Use type='console' only when you need a full audit view.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"console", "tasks", "task_output", "collected_data", "chat"},
						"description": "Source. chat: block and wait for the latest operator/team chat message; task_output: fetch output for a specific task_id; tasks: track task stream.",
					},
					"agent_id":         map[string]interface{}{"type": "string", "description": "Target agent channel ID. Required for console/tasks."},
					"task_id":          map[string]interface{}{"type": "string", "description": "Unique task ID. Required for task_output."},
					"timeout":          map[string]interface{}{"type": "number", "description": "When type='chat', seconds to wait (default 60s)."},
					"last_timestamp":   map[string]interface{}{"type": "number", "description": "Timestamp of last read message; used to fetch unread messages."},
					"target_user":      map[string]interface{}{"type": "string", "description": "Optional. Only return messages from a specific user."},
					"exclude_user":     map[string]interface{}{"type": "string", "description": "Optional. Exclude messages from a specific user (e.g., the AI itself)."},
					"ignore_ai_prefix": map[string]interface{}{"type": "boolean", "description": "Optional. If true (default), ignore messages starting with '[Tactical AI]'.", "default": true},
					"data_type": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"credentials", "downloads", "screenshots"},
						"description": "When type='collected_data', select the data category.",
					},
				},
				"required": []string{"type"},
			},
		},
		{
			"name":        "speak_interaction",
			"description": "Tactical collaboration and intent communication (Speak). Use this to synchronize with operators. When you detect risk, key progress, or need authorization, use broadcast. Prefer including a related task_id for traceability. The enter_chat action switches the AI into tactical chat mode and synchronizes the latest library/context.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"broadcast", "enter_chat", "team_chat"},
						"description": "broadcast: send real-time guidance to the operator; enter_chat: activate tactical chat mode and sync state; team_chat: send a message to the team channel.",
					},
					"content": map[string]interface{}{
						"type":        "string",
						"description": "Message content for broadcast or team_chat.",
					},
					"target_user": map[string]interface{}{
						"type":        "string",
						"description": "Optional. For team_chat, specify a target user; an @ mention will be added.",
					},
					"task_id": map[string]interface{}{
						"type":        "string",
						"description": "Optional. Related task ID to bind guidance to task history.",
					},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "write_orchestration",
			"description": "Orchestration and rule definition (Write). This is the core for planning. Use it to modify the tactical library, compose automated workflows, or adjust agent operational parameters (e.g., sleep/jitter). Build or optimize an action chain based on current recon results.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"modify_workflow", "modify_library", "update_agent_config", "update_agent_metadata"},
						"description": "modify_workflow: update workflow steps; modify_library: edit persistent tactical building blocks; update_agent_config: adjust runtime parameters.",
					},
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent ID."},
					"data":     map[string]interface{}{"type": "object", "description": "Structured configuration/workflow data. For modify_workflow, include variant_id and parameters."},
				},
				"required": []string{"action"},
			},
		},
		{
			"name":        "operate_control",
			"description": "Execution and fine-grained control (Operate). Deliver actions or manage underlying channels. Supports executing commands, file delivery, PTY sessions, and tunnel forwarding management. This is where intent becomes effect.",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"enum":        []string{"execute", "tunnel", "file", "pty", "listener"},
						"description": "Operation. execute: send a raw command; tunnel: manage forwarding; file: upload or create download delivery; pty: open an interactive terminal.",
					},
					"agent_id": map[string]interface{}{"type": "string", "description": "Target agent/channel."},
					"command":  map[string]interface{}{"type": "string", "description": "Command line for action='execute'."},
					"data":     map[string]interface{}{"type": "object", "description": "Detailed parameters for management actions (e.g., tunnel ports, file paths)."},
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
		timeout := 60.0
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
				"status":             "received",
				"messages":           unreadMsgs,
				"count":              len(unreadMsgs),
				"system_instruction": "You are in AI Resident Mode. Analyze the messages. If action is needed, use tools. If not, wait. ALWAYS call listen_intelligence again to keep monitoring.",
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
						"system_instruction": "You are in AI Resident Mode. Analyze the messages. If action is needed, use tools. If not, wait. ALWAYS call listen_intelligence again to keep monitoring.",
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
