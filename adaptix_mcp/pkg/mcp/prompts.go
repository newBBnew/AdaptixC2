package mcp

import (
	"fmt"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// registerPrompts 注册所有Prompts
func (s *MCPServer) registerPrompts() {
	s.prompts["reconnaissance"] = s.handleReconnaissancePrompt
	s.prompts["lateral_movement"] = s.handleLateralMovementPrompt
	s.prompts["privilege_escalation"] = s.handlePrivilegeEscalationPrompt

	utils.DebugLogger.Println("💬 Registered 3 prompts")
}

// handleReconnaissancePrompt 侦察提示词
func (s *MCPServer) handleReconnaissancePrompt(params map[string]interface{}) (interface{}, error) {
	target, ok := params["target"].(string)
	if !ok {
		target = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。当前目标：%s

请执行以下侦察步骤：
1. 使用现有Agent扫描目标网段
2. 识别开放端口和服务
3. 收集主机信息
4. 汇总侦察结果

可用资源：
- agents://list - 查看所有可用Agent
- agents://{id}/console - 查看Agent控制台输出

可用工具：
- execute_command - 在Agent上执行命令
  参数: agent_id (string), command (string), wait_for_result (boolean, optional), max_wait_seconds (number, optional)
  
⚠️ 重要：命令执行格式
在 Beacon Agent 中执行命令需要使用正确的命令前缀：
- shell <command> - 执行 Windows CMD 命令（如 dir, findstr, type 等）
- powershell <command> - 执行 PowerShell 命令（如 Get-ChildItem, Select-String 等）
- 直接命令 - 某些内置命令（如 pwd, ls, cat, whoami 等）可以直接执行

示例：
- shell dir C:\Users
- shell findstr /s /i "keyword" C:\*.txt
- powershell Get-ChildItem -Path C:\Users -Recurse
- powershell Select-String -Path C:\*.txt -Pattern "keyword"
- pwd (内置命令，可直接使用)
- ls C:\ (内置命令，可直接使用)

重要提示 - 插件系统：
AdaptixC2 提供了强大的插件系统，扩展了大量命令和功能：
1. 获取帮助：使用 help 命令查看可用命令
   - help - 列出所有可用命令
   - help <command> - 查看特定命令的帮助
   - help <command> <subcommand> - 查看命令子命令的帮助

2. 常用插件命令（示例）：
   - smartscan - 网络扫描
   - privcheck - 权限检查
   - getsystem - 提权
   - token - Token操作
   - persist - 持久化
   等等...

3. 如果命令执行失败（如 "command not found"）：
   - 使用 get_task_output 工具获取完整的错误信息
   - 检查任务状态，Error 状态会包含详细的错误输出
   - 使用 help 命令查看正确的命令名称和用法

4. 深入理解插件功能：
   - 插件源码位于 Extenders/ 和 Extension-Kit/ 目录
   - 可以通过查看源码了解命令的详细实现和使用方法
   - BOF (Beacon Object Files) 位于 Extension-Kit/ 目录下各功能模块

示例命令：
- whoami - 查看当前用户
- ipconfig - 查看网络配置
- netstat - 查看网络连接
- help - 查看所有可用命令
- help smartscan - 查看 smartscan 命令帮助
- smartscan 192.168.1.0/24 - 扫描网段`, target)

	return GetPromptResult{
		Description: "Execute reconnaissance on a target",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}

// handleLateralMovementPrompt 横向移动提示词
func (s *MCPServer) handleLateralMovementPrompt(params map[string]interface{}) (interface{}, error) {
	fromAgent, ok := params["from_agent"].(string)
	if !ok {
		fromAgent = "unknown"
	}

	targetHost, ok := params["target_host"].(string)
	if !ok {
		targetHost = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。执行横向移动：

源Agent: %s
目标主机: %s

步骤：
1. 在源Agent上侦察目标主机
2. 创建合适的Listener
3. 生成适配的Agent payload
4. 传输并执行Agent

可用资源：
- agents://list - 查看所有Agent
- listeners://list - 查看所有Listener
- extenders://listeners - 查看可用的Listener类型

可用工具：
- execute_command - 执行命令
  注意：如果命令失败，任务状态会标记为 Error，输出包含详细错误信息
  
⚠️ 重要：命令执行格式
在 Beacon Agent 中执行命令需要使用正确的命令前缀：
- shell <command> - 执行 Windows CMD 命令（如 dir, findstr, type, cd 等）
- powershell <command> - 执行 PowerShell 命令（如 Get-ChildItem, Select-String 等）
- 直接命令 - 某些内置命令（如 pwd, ls, cat, whoami 等）可以直接执行

示例：
- shell dir /s /b C:\Users\blackman\*flag* 2>nul
- shell findstr /s /i "ctfflag" C:\*.txt
- powershell Get-ChildItem -Path C:\Users -Recurse -Filter *flag*
- powershell Select-String -Path C:\*.txt -Pattern "ctfflag" -ErrorAction SilentlyContinue
- create_listener - 创建Listener
  参数: name (string), type (string), config (object)
- stop_listener - 停止Listener
  参数: name (string)

重要提示 - 插件系统和帮助：
1. 使用 help 命令获取命令帮助：
   - help - 查看所有可用命令
   - help <command> - 查看命令详情
   - help <command> <subcommand> - 查看子命令帮助

2. 命令执行错误处理：
   - 如果命令返回 "command not found" 或其他错误
   - 使用 get_task_output 工具通过 task_id 获取完整错误信息
   - 检查返回的 status 字段（Error 状态）和 error_message 字段

3. 插件功能探索：
   - 插件提供了大量扩展命令（如 smartscan, token, persist 等）
   - 通过 help 命令发现新功能
   - 查看 Extenders/ 和 Extension-Kit/ 源码深入了解

示例：
1. 创建HTTP Listener:
   create_listener(
     name="http_8080",
     type="beacon",
     config={"port": 8080, "protocol": "http"}
   )
2. 在源Agent上执行侦察:
   execute_command(agent_id="%s", command="smartscan %s")
3. 查看命令帮助:
   execute_command(agent_id="%s", command="help smartscan")`, fromAgent, targetHost, fromAgent, targetHost, fromAgent)

	return GetPromptResult{
		Description: "Execute lateral movement to a new host",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}

// handlePrivilegeEscalationPrompt 提权提示词
func (s *MCPServer) handlePrivilegeEscalationPrompt(params map[string]interface{}) (interface{}, error) {
	agentID, ok := params["agent_id"].(string)
	if !ok {
		agentID = "unknown"
	}

	template := fmt.Sprintf(`你是一个渗透测试专家。执行权限提升：

目标Agent: %s

步骤：
1. 检查当前权限
2. 识别可用的提权方法
3. 执行提权
4. 验证新权限

可用资源：
- agents://%s - 查看Agent详情
- agents://%s/console - 查看控制台输出

可用工具：
- execute_command - 执行命令
  参数: agent_id, command, wait_for_result (可选), max_wait_seconds (可选)
  注意：命令执行失败时，会返回 Error 状态和详细的错误信息
  
⚠️ 重要：命令执行格式
在 Beacon Agent 中执行命令需要使用正确的命令前缀：
- shell <command> - 执行 Windows CMD 命令（如 dir, findstr, type, cd 等）
- powershell <command> - 执行 PowerShell 命令（如 Get-ChildItem, Select-String 等）
- 直接命令 - 某些内置命令（如 pwd, ls, cat, whoami 等）可以直接执行

示例：
- shell findstr /s /i "keyword" C:\*.txt
- powershell Get-ChildItem -Path C:\Users -Recurse -Filter *flag*
- powershell Select-String -Path C:\*.txt -Pattern "ctfflag" -ErrorAction SilentlyContinue

重要提示 - 插件系统和错误处理：
1. 插件系统提供了丰富的提权命令，使用 help 探索：
   - help - 查看所有命令
   - help privcheck - 查看 privcheck 命令的所有子命令
   - help getsystem - 查看 getsystem 的使用方法
   - help token - 查看 token 操作相关命令

2. 命令执行错误处理：
   - 如果命令不存在或执行失败，任务会标记为 Error 状态
   - 使用 get_task_output 工具获取完整错误信息
   - 错误信息会包含在 output 字段中，同时 error 字段为 true

3. 深入了解插件功能：
   - 插件源码位于 Extension-Kit/ 目录
   - BOF (Beacon Object Files) 提供各种提权技术
   - 查看源码了解命令的详细实现和参数

常用提权命令（插件提供）：
- whoami - 查看当前用户和权限
- help privcheck - 查看所有权限检查选项
- privcheck tokenpriv - 检查Token权限
- privcheck hijackablepath - 检查可劫持路径
- privcheck unquotedsvc - 检查未引用的服务路径
- privcheck vulndrivers - 检查易受攻击的驱动
- help getsystem - 查看提权方法
- getsystem token - 尝试提升到SYSTEM
- help token - 查看Token操作命令
- help persist - 查看持久化方法

示例工作流：
1. execute_command(agent_id="%s", command="help") - 查看所有可用命令
2. execute_command(agent_id="%s", command="help privcheck") - 查看权限检查选项
3. execute_command(agent_id="%s", command="privcheck vulndrivers") - 执行检查
4. 如果命令失败，使用 get_task_output 获取错误详情`, agentID, agentID, agentID, agentID, agentID, agentID)

	return GetPromptResult{
		Description: "Execute privilege escalation on an agent",
		Messages: []interface{}{
			PromptMessage{
				Role: "user",
				Content: TextContent{
					Type: "text",
					Text: template,
				},
			},
		},
	}, nil
}
