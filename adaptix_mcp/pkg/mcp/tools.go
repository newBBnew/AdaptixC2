package mcp

import (
	"fmt"
	"time"
)

func (s *MCPServer) registerTools() {
	s.tools["list_agents"] = s.handleListAgents
	s.tools["get_agent_info"] = s.handleGetAgentInfo
	s.tools["execute_command"] = s.handleExecuteCommand
	s.tools["get_console_output"] = s.handleGetConsoleOutput
	s.tools["clear_console"] = s.handleClearConsole
	s.tools["list_tasks"] = s.handleListTasks
	s.tools["get_task_output"] = s.handleGetTaskOutput
	s.tools["delete_tasks"] = s.handleDeleteTasks
	s.tools["list_listeners"] = s.handleListListeners
	s.tools["manage_listener"] = s.handleManageListener
	s.tools["list_tunnels"] = s.handleListTunnels
	s.tools["manage_tunnel"] = s.handleManageTunnel
	s.tools["list_targets"] = s.handleListTargets
	s.tools["manage_target"] = s.handleManageTarget
	s.tools["list_pivots"] = s.handleListPivots
	s.tools["list_collected_data"] = s.handleListCollectedData
	s.tools["list_filedelivery"] = s.handleListFileDelivery
	s.tools["manage_filedelivery"] = s.handleManageFileDelivery
	s.tools["update_agent_config"] = s.handleUpdateAgentConfig
	s.tools["update_agent_metadata"] = s.handleUpdateAgentMetadata
	s.tools["execute_and_wait"] = s.handleExecuteAndWait
	s.tools["manage_pty"] = s.handleManagePty
}

func (s *MCPServer) getToolDefinitions() []map[string]interface{} {
	return []map[string]interface{}{
		{"name": "list_agents", "description": "List all connected agents", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "get_agent_info", "description": "Get detailed agent information", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string", "description": "Agent ID"}}, "required": []string{"agent_id"}}},
		{"name": "execute_command", "description": "Execute command on agent", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}, "command": map[string]interface{}{"type": "string"}}, "required": []string{"agent_id", "command"}}},
		{"name": "get_console_output", "description": "Get agent console output", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}}, "required": []string{"agent_id"}}},
		{"name": "clear_console", "description": "Clear agent console", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}}, "required": []string{"agent_id"}}},
		{"name": "list_tasks", "description": "List tasks (optionally filtered by agent)", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string", "description": "Optional agent ID filter"}}}},
		{"name": "get_task_output", "description": "Get task output", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"task_id": map[string]interface{}{"type": "string"}}, "required": []string{"task_id"}}},
		{"name": "delete_tasks", "description": "Delete tasks", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"task_id": map[string]interface{}{"type": "string"}, "task_ids": map[string]interface{}{"type": "array", "items": map[string]interface{}{"type": "string"}}}}},
		{"name": "list_listeners", "description": "List all listeners", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "manage_listener", "description": "Manage listeners (start, stop, edit). GUIDANCE: For 'start', provide 'name' (e.g., 'http_80'), 'type' (e.g., 'BeaconHTTP'), and 'data' as a JSON config string containing 'host_bind', 'port_bind', etc. For 'edit', existing name and updated config are required.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"action": map[string]interface{}{"type": "string", "enum": []string{"start", "stop", "edit"}}, "name": map[string]interface{}{"type": "string"}, "type": map[string]interface{}{"type": "string", "description": "Listener type (e.g. BeaconHTTP, BeaconTCP)"}, "data": map[string]interface{}{"type": "string", "description": "JSON configuration string for start/edit action"}}, "required": []string{"action", "name"}}},
		{"name": "list_tunnels", "description": "List all active tunnels (socks4/5, port forward). Use this to find 'tunnel_id' for management.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "manage_tunnel", "description": "Manage tunnels (start, stop, edit). GUIDANCE: 'start' requires 'type' (socks5, socks4, lportfwd, rportfwd) and 'data' object. For socks5 start, data needs 'l_host', 'l_port', 'agent_id'. 'edit' updates info field using 'tunnel_id'.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"action": map[string]interface{}{"type": "string", "enum": []string{"start", "stop", "edit"}}, "type": map[string]interface{}{"type": "string", "description": "Tunnel type (e.g. socks5, socks4, lportfwd, rportfwd)"}, "data": map[string]interface{}{"type": "object", "description": "Configuration object for start action"}, "tunnel_id": map[string]interface{}{"type": "string", "description": "Tunnel ID for stop/edit action"}, "info": map[string]interface{}{"type": "string", "description": "New info/description for edit action"}}, "required": []string{"action"}}},
		{"name": "list_targets", "description": "List discovered targets", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "manage_target", "description": "Manage discovered targets (remove)", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"action": map[string]interface{}{"type": "string", "enum": []string{"remove"}}, "target_id": map[string]interface{}{"type": "string", "description": "Single target ID"}, "target_ids": map[string]interface{}{"type": "array", "items": map[string]interface{}{"type": "string"}, "description": "List of target IDs"}}, "required": []string{"action"}}},
		{"name": "list_pivots", "description": "List pivots", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "list_collected_data", "description": "List collected data (credentials, downloads, screenshots)", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"data_type": map[string]interface{}{"type": "string", "enum": []string{"credentials", "downloads", "screenshots"}}}, "required": []string{"data_type"}}},
		{"name": "list_filedelivery", "description": "List all hosted files for delivery. Use this to find 'file_id' for sharing or deletion.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "manage_filedelivery", "description": "Manage hosted files. GUIDANCE: 'upload' needs 'local_path' (and optional 'file_name'). 'delete' needs 'file_id'. 'create_link' needs 'file_id' and optional 'expire_hours', 'max_uses', 'allowed_ip'.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"action": map[string]interface{}{"type": "string", "enum": []string{"upload", "delete", "create_link"}}, "local_path": map[string]interface{}{"type": "string", "description": "Local path of the file to upload"}, "file_name": map[string]interface{}{"type": "string", "description": "Name to give the file on the server"}, "file_id": map[string]interface{}{"type": "string", "description": "ID of the hosted file"}, "expire_hours": map[string]interface{}{"type": "number", "description": "Link expiration time in hours (default 24)"}, "max_uses": map[string]interface{}{"type": "number", "description": "Maximum number of downloads (0 for unlimited)"}, "allowed_ip": map[string]interface{}{"type": "string", "description": "Restricts downloads to this single IP"}}, "required": []string{"action"}}},
		{"name": "update_agent_config", "description": "Update agent sleep/jitter", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}, "sleep": map[string]interface{}{"type": "number"}, "jitter": map[string]interface{}{"type": "number"}}, "required": []string{"agent_id"}}},
		{"name": "update_agent_metadata", "description": "Update agent tag/mark", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}, "metadata_type": map[string]interface{}{"type": "string", "enum": []string{"tag", "mark"}}, "value": map[string]interface{}{"type": "string"}}, "required": []string{"agent_id", "metadata_type"}}},
		{"name": "execute_and_wait", "description": "Execute command and wait for result (recommended for getting command output)", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string", "description": "Agent ID"}, "command": map[string]interface{}{"type": "string", "description": "Command to execute"}, "timeout": map[string]interface{}{"type": "number", "description": "Timeout in seconds (default 60)"}}, "required": []string{"agent_id", "command"}}},
		{"name": "manage_pty", "description": "Manage interactive PTY sessions. GUIDANCE: 'open' needs 'agent_id' and optional 'program'. Use 'list' to find active 'pty_id's. 'read' output is automatically cleaned of ANSI codes. Always 'close' sessions when task is done.", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"action": map[string]interface{}{"type": "string", "enum": []string{"open", "read", "write", "close", "list"}}, "agent_id": map[string]interface{}{"type": "string", "description": "Agent ID (required for open)"}, "pty_id": map[string]interface{}{"type": "string", "description": "PTY session ID (required for read, write, close)"}, "program": map[string]interface{}{"type": "string", "description": "Program to run (optional for open)"}, "data": map[string]interface{}{"type": "string", "description": "Data to write (required for write)"}, "base64": map[string]interface{}{"type": "boolean", "description": "If data is base64 encoded (optional for write)"}, "clear": map[string]interface{}{"type": "boolean", "description": "Clear buffer after reading (optional for read)"}, "rows": map[string]interface{}{"type": "number", "description": "Terminal rows (optional for open)"}, "cols": map[string]interface{}{"type": "number", "description": "Terminal columns (optional for open)"}}, "required": []string{"action"}}},
	}
}

func (s *MCPServer) handleListAgents(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_agents", params)
}

func (s *MCPServer) handleGetAgentInfo(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_agent_info", params)
}

func (s *MCPServer) handleExecuteCommand(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("execute_command", params)
}

func (s *MCPServer) handleGetConsoleOutput(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("get_console_output", params)
}

func (s *MCPServer) handleClearConsole(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("clear_console", params)
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

func (s *MCPServer) handleUpdateAgentConfig(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("update_agent_config", params)
}

func (s *MCPServer) handleUpdateAgentMetadata(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("update_agent_metadata", params)
}

func (s *MCPServer) handleListFileDelivery(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_filedelivery", params)
}

func (s *MCPServer) handleManageFileDelivery(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_filedelivery", params)
}

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
					// Task completed, return result
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

	// Timeout
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

func (s *MCPServer) handleManagePty(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("manage_pty", params)
}
