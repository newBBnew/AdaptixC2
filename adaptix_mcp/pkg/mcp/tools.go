package mcp

import (
	"fmt"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// registerTools 注册所有Tools
func (s *MCPServer) registerTools() {
	s.tools["execute_command"] = s.handleExecuteCommandTool
	s.tools["get_console_output"] = s.handleGetConsoleOutputTool
	s.tools["clear_console"] = s.handleClearConsoleTool
	s.tools["list_agents"] = s.handleListAgentsTool
	s.tools["get_agent_info"] = s.handleGetAgentInfoTool
	s.tools["list_listeners"] = s.handleListListenersTool
	s.tools["create_listener"] = s.handleCreateListenerTool
	s.tools["edit_listener"] = s.handleEditListenerTool
	s.tools["stop_listener"] = s.handleStopListenerTool
	s.tools["list_credentials"] = s.handleListCredentialsTool
	s.tools["list_downloads"] = s.handleListDownloadsTool
	s.tools["list_screenshots"] = s.handleListScreenshotsTool
	s.tools["list_tasks"] = s.handleListTasksTool
	s.tools["get_task_output"] = s.handleGetTaskOutputTool
	s.tools["list_tunnels"] = s.handleListTunnelsTool
	s.tools["create_tunnel"] = s.handleCreateTunnelTool
	s.tools["stop_tunnel"] = s.handleStopTunnelTool
	s.tools["remove_agent"] = s.handleRemoveAgentTool
	s.tools["update_agent_config"] = s.handleUpdateAgentConfigTool
	s.tools["set_agent_tag"] = s.handleSetAgentTagTool
	s.tools["set_agent_mark"] = s.handleSetAgentMarkTool
	s.tools["list_targets"] = s.handleListTargetsTool
	s.tools["list_pivots"] = s.handleListPivotsTool

	utils.InfoLogger.Println("🛠️  Registered Tools: execute_command, get_console_output, clear_console, list_agents, get_agent_info, list_listeners, create_listener, edit_listener, stop_listener, list_credentials, list_downloads, list_screenshots, list_tasks, get_task_output, list_tunnels, create_tunnel, stop_tunnel, remove_agent, update_agent_config, set_agent_tag, set_agent_mark, list_targets, list_pivots")
}

// routeTool 路由Tool请求
func (s *MCPServer) routeTool(name string, params map[string]interface{}) (CallToolResult, error) {
	handler, ok := s.tools[name]
	if !ok {
		return CallToolResult{}, fmt.Errorf("unknown tool: %s", name)
	}

	// 调用Handler
	data, err := handler(params)
	if err != nil {
		return CallToolResult{
			Content: []interface{}{
				TextContent{
					Type: "text",
					Text: fmt.Sprintf("Error: %v", err),
				},
			},
			IsError: true,
		}, nil
	}

	// 将结果转换为文本
	text := fmt.Sprintf("%v", data)

	return CallToolResult{
		Content: []interface{}{
			TextContent{
				Type: "text",
				Text: text,
			},
		},
		IsError: false,
	}, nil
}

// handleExecuteCommandTool 执行命令
func (s *MCPServer) handleExecuteCommandTool(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid agent_id")
	}

	command, ok := params["command"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid command")
	}

	// 调用ConsoleHandler
	resp, err := s.clientConnector.SendCommand("console", map[string]interface{}{
		"command":  "send_input",
		"agent_id": agentID,
		"input":    command,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to execute command: %w", err)
	}

	return fmt.Sprintf("✅ Command sent to agent %s: %s\nMessage: %s", agentID, command, resp.Message), nil
}

// handleGetConsoleOutputTool 获取控制台输出
func (s *MCPServer) handleGetConsoleOutputTool(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid agent_id")
	}

	// 调用InfoHandler获取控制台输出
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command":  "get_agent_console",
		"agent_id": agentID,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get console output: %w", err)
	}

	// 从响应中提取console_output
	if resp.Data != nil {
		if consoleOutput, ok := resp.Data["console_output"].(string); ok {
			return consoleOutput, nil
		}
	}

	return "", fmt.Errorf("no console output available")
}

// handleClearConsoleTool 清空控制台输出
func (s *MCPServer) handleClearConsoleTool(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid agent_id")
	}

	// 调用ConsoleHandler清空控制台
	resp, err := s.clientConnector.SendCommand("console", map[string]interface{}{
		"command":  "clear_console",
		"agent_id": agentID,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to clear console: %w", err)
	}

	// 返回成功消息
	if resp.Status == "success" {
		return fmt.Sprintf("✅ Console cleared for agent %s", agentID), nil
	}

	return nil, fmt.Errorf("failed to clear console: %s", resp.Message)
}

// handleListAgentsTool 列出所有Agent
func (s *MCPServer) handleListAgentsTool(params map[string]interface{}) (interface{}, error) {
	// 调用InfoHandler获取Agent列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "list_agents",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list agents: %w", err)
	}

	// 从响应中提取agents数据
	if resp.Data != nil {
		if agents, ok := resp.Data["agents"]; ok {
			return agents, nil
		}
	}

	return []interface{}{}, nil
}

// handleGetAgentInfoTool 获取Agent详细信息
func (s *MCPServer) handleGetAgentInfoTool(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid agent_id")
	}

	// 调用InfoHandler获取Agent信息
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command":  "get_agent_info",
		"agent_id": agentID,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get agent info: %w", err)
	}

	// 返回整个data对象
	if resp.Data != nil {
		return resp.Data, nil
	}

	return map[string]interface{}{}, fmt.Errorf("no agent info available")
}

// handleListTasksTool 列出所有任务
func (s *MCPServer) handleListTasksTool(params map[string]interface{}) (interface{}, error) {
	// Optional agent_id filter
	reqParams := map[string]interface{}{
		"command": "list_tasks",
	}

	if agentID, ok := params["agent_id"].(string); ok && agentID != "" {
		reqParams["agent_id"] = agentID
	}

	resp, err := s.clientConnector.SendCommand("info", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to list tasks: %w", err)
	}

	if resp.Data != nil {
		return resp.Data, nil
	}

	return map[string]interface{}{
		"tasks": []interface{}{},
		"count": 0,
	}, nil
}

// handleGetTaskOutputTool 获取指定任务的完整输出
func (s *MCPServer) handleGetTaskOutputTool(params map[string]interface{}) (interface{}, error) {
	taskID, ok := params["task_id"].(string)
	if !ok || taskID == "" {
		return nil, fmt.Errorf("missing or invalid task_id")
	}

	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "get_task_output",
		"task_id": taskID,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get task output: %w", err)
	}

	if resp.Data != nil {
		return resp.Data, nil
	}

	return nil, fmt.Errorf("task not found: %s", taskID)
}

// handleListListenersTool 列出所有Listener
func (s *MCPServer) handleListListenersTool(params map[string]interface{}) (interface{}, error) {
	// 调用ListenerHandler获取Listener列表
	resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
		"command": "list",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list listeners: %w", err)
	}

	// 从响应中提取listeners数据
	if resp.Data != nil {
		if listeners, ok := resp.Data["listeners"]; ok {
			return listeners, nil
		}
	}

	return []interface{}{}, nil
}

// handleListCredentialsTool 列出所有凭证
func (s *MCPServer) handleListCredentialsTool(params map[string]interface{}) (interface{}, error) {
	// 调用InfoHandler获取凭证列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "list_credentials",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list credentials: %w", err)
	}

	// 从响应中提取credentials数据
	if resp.Data != nil {
		if credentials, ok := resp.Data["credentials"]; ok {
			return credentials, nil
		}
	}

	return []interface{}{}, nil
}

// handleListDownloadsTool 列出所有下载
func (s *MCPServer) handleListDownloadsTool(params map[string]interface{}) (interface{}, error) {
	// 调用InfoHandler获取下载列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "list_downloads",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list downloads: %w", err)
	}

	// 从响应中提取downloads数据
	if resp.Data != nil {
		if downloads, ok := resp.Data["downloads"]; ok {
			return downloads, nil
		}
	}

	return []interface{}{}, nil
}

// handleListScreenshotsTool 列出所有截图
func (s *MCPServer) handleListScreenshotsTool(params map[string]interface{}) (interface{}, error) {
	// 调用InfoHandler获取截图列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": "list_screenshots",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list screenshots: %w", err)
	}

	// 从响应中提取screenshots数据
	if resp.Data != nil {
		if screenshots, ok := resp.Data["screenshots"]; ok {
			return screenshots, nil
		}
	}

	return []interface{}{}, nil
}

// handleCreateListenerTool 创建Listener
func (s *MCPServer) handleCreateListenerTool(params map[string]interface{}) (interface{}, error) {
	name, ok := params["name"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid name")
	}

	listenerType, ok := params["type"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid type")
	}

	config, ok := params["config"].(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("missing or invalid config")
	}

	// 调用ListenerHandler
	resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
		"command":       "create",
		"name":          name,
		"listener_type": listenerType,
		"config":        config,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create listener: %w", err)
	}

	return fmt.Sprintf("✅ Listener created: %s (type: %s)\nMessage: %s", name, listenerType, resp.Message), nil
}

// handleEditListenerTool 编辑Listener
func (s *MCPServer) handleEditListenerTool(params map[string]interface{}) (interface{}, error) {
	name, ok := params["name"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid name")
	}

	config, ok := params["config"].(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("missing or invalid config")
	}

	// 调用ListenerHandler
	resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
		"command": "edit",
		"name":    name,
		"config":  config,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to edit listener: %w", err)
	}

	return fmt.Sprintf("✅ Listener edited: %s\nMessage: %s", name, resp.Message), nil
}

// handleStopListenerTool 停止Listener
func (s *MCPServer) handleStopListenerTool(params map[string]interface{}) (interface{}, error) {
	name, ok := params["name"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid name")
	}

	// 调用ListenerHandler
	resp, err := s.clientConnector.SendCommand("listener", map[string]interface{}{
		"command": "stop",
		"name":    name,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to stop listener: %w", err)
	}

	return fmt.Sprintf("✅ Listener stopped: %s\nMessage: %s", name, resp.Message), nil
}

// handleListTunnelsTool 列出所有Tunnel
func (s *MCPServer) handleListTunnelsTool(params map[string]interface{}) (interface{}, error) {
	// 调用TunnelHandler
	resp, err := s.clientConnector.SendCommand("tunnel", map[string]interface{}{
		"command": "list",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list tunnels: %w", err)
	}

	if resp.Data != nil {
		return resp.Data, nil
	}

	return map[string]interface{}{
		"tunnels": []interface{}{},
		"total":   0,
	}, nil
}

// handleCreateTunnelTool 创建Tunnel
func (s *MCPServer) handleCreateTunnelTool(params map[string]interface{}) (interface{}, error) {
	tunnelType, ok := params["tunnel_type"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid tunnel_type")
	}

	config, ok := params["config"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid config")
	}

	// 调用TunnelHandler
	resp, err := s.clientConnector.SendCommand("tunnel", map[string]interface{}{
		"command":     "create",
		"tunnel_type": tunnelType,
		"config":      config,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create tunnel: %w", err)
	}

	return fmt.Sprintf("✅ Tunnel created: %s\nMessage: %s", tunnelType, resp.Message), nil
}

// handleStopTunnelTool 停止Tunnel
func (s *MCPServer) handleStopTunnelTool(params map[string]interface{}) (interface{}, error) {
	tunnelID, ok := params["tunnel_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid tunnel_id")
	}

	// 调用TunnelHandler
	resp, err := s.clientConnector.SendCommand("tunnel", map[string]interface{}{
		"command":   "stop",
		"tunnel_id": tunnelID,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to stop tunnel: %w", err)
	}

	return fmt.Sprintf("✅ Tunnel stopped: %s\nMessage: %s", tunnelID, resp.Message), nil
}

// handleRemoveAgentTool 删除Agent
func (s *MCPServer) handleRemoveAgentTool(params map[string]interface{}) (interface{}, error) {
	// Support both agent_id and agent_ids
	reqParams := map[string]interface{}{
		"command": "remove",
	}

	if agentID, ok := params["agent_id"].(string); ok && agentID != "" {
		reqParams["agent_id"] = agentID
	} else if agentIDs, ok := params["agent_ids"].([]interface{}); ok && len(agentIDs) > 0 {
		reqParams["agent_ids"] = agentIDs
	} else {
		return nil, fmt.Errorf("missing agent_id or agent_ids")
	}

	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to remove agent: %w", err)
	}

	return fmt.Sprintf("✅ Agent(s) removed\nMessage: %s", resp.Message), nil
}

// handleUpdateAgentConfigTool 更新Agent配置
func (s *MCPServer) handleUpdateAgentConfigTool(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok || agentID == "" {
		return nil, fmt.Errorf("missing or invalid agent_id")
	}

	reqParams := map[string]interface{}{
		"command":  "update_config",
		"agent_id": agentID,
	}

	if sleep, ok := params["sleep"]; ok {
		reqParams["sleep"] = sleep
	}

	if jitter, ok := params["jitter"]; ok {
		reqParams["jitter"] = jitter
	}

	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to update agent config: %w", err)
	}

	return fmt.Sprintf("✅ Agent config updated: %s\nMessage: %s", agentID, resp.Message), nil
}

// handleSetAgentTagTool 设置Agent标签
func (s *MCPServer) handleSetAgentTagTool(params map[string]interface{}) (interface{}, error) {
	reqParams := map[string]interface{}{
		"command": "set_tag",
	}

	if agentID, ok := params["agent_id"].(string); ok && agentID != "" {
		reqParams["agent_id"] = agentID
	} else if agentIDs, ok := params["agent_ids"].([]interface{}); ok && len(agentIDs) > 0 {
		reqParams["agent_ids"] = agentIDs
	} else {
		return nil, fmt.Errorf("missing agent_id or agent_ids")
	}

	tag, _ := params["tag"].(string) // Tag can be empty
	reqParams["tag"] = tag

	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to set agent tag: %w", err)
	}

	return fmt.Sprintf("✅ Agent tag set\nMessage: %s", resp.Message), nil
}

// handleSetAgentMarkTool 设置Agent标记
func (s *MCPServer) handleSetAgentMarkTool(params map[string]interface{}) (interface{}, error) {
	reqParams := map[string]interface{}{
		"command": "set_mark",
	}

	if agentID, ok := params["agent_id"].(string); ok && agentID != "" {
		reqParams["agent_id"] = agentID
	} else if agentIDs, ok := params["agent_ids"].([]interface{}); ok && len(agentIDs) > 0 {
		reqParams["agent_ids"] = agentIDs
	} else {
		return nil, fmt.Errorf("missing agent_id or agent_ids")
	}

	mark, _ := params["mark"].(string) // Mark can be empty
	reqParams["mark"] = mark

	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to set agent mark: %w", err)
	}

	return fmt.Sprintf("✅ Agent mark set\nMessage: %s", resp.Message), nil
}

// handleListTargetsTool 列出所有Target
func (s *MCPServer) handleListTargetsTool(params map[string]interface{}) (interface{}, error) {
	resp, err := s.clientConnector.SendCommand("targets", map[string]interface{}{
		"command": "list",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list targets: %w", err)
	}

	if resp.Data != nil {
		return resp.Data, nil
	}

	return map[string]interface{}{
		"targets": []interface{}{},
		"total":   0,
	}, nil
}

// handleListPivotsTool 列出所有Pivot
func (s *MCPServer) handleListPivotsTool(params map[string]interface{}) (interface{}, error) {
	resp, err := s.clientConnector.SendCommand("pivots", map[string]interface{}{
		"command": "list",
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list pivots: %w", err)
	}

	if resp.Data != nil {
		return resp.Data, nil
	}

	return map[string]interface{}{
		"pivots": []interface{}{},
		"total":  0,
	}, nil
}
