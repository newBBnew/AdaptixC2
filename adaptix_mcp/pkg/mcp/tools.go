package mcp

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
	s.tools["list_tunnels"] = s.handleListTunnels
	s.tools["list_targets"] = s.handleListTargets
	s.tools["list_pivots"] = s.handleListPivots
	s.tools["list_collected_data"] = s.handleListCollectedData
	s.tools["update_agent_config"] = s.handleUpdateAgentConfig
	s.tools["update_agent_metadata"] = s.handleUpdateAgentMetadata
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
		{"name": "list_tunnels", "description": "List all tunnels", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "list_targets", "description": "List discovered targets", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "list_pivots", "description": "List pivots", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{}}},
		{"name": "list_collected_data", "description": "List collected data (credentials, downloads, screenshots)", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"data_type": map[string]interface{}{"type": "string", "enum": []string{"credentials", "downloads", "screenshots"}}}, "required": []string{"data_type"}}},
		{"name": "update_agent_config", "description": "Update agent sleep/jitter", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}, "sleep": map[string]interface{}{"type": "number"}, "jitter": map[string]interface{}{"type": "number"}}, "required": []string{"agent_id"}}},
		{"name": "update_agent_metadata", "description": "Update agent tag/mark", "inputSchema": map[string]interface{}{"type": "object", "properties": map[string]interface{}{"agent_id": map[string]interface{}{"type": "string"}, "metadata_type": map[string]interface{}{"type": "string", "enum": []string{"tag", "mark"}}, "value": map[string]interface{}{"type": "string"}}, "required": []string{"agent_id", "metadata_type"}}},
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

func (s *MCPServer) handleListTunnels(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_tunnels", params)
}

func (s *MCPServer) handleListTargets(params map[string]interface{}) (interface{}, error) {
	return s.clientConnector.SendCommand("list_targets", params)
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
