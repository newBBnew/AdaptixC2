package mcp

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

// MCPServer MCP服务器
type MCPServer struct {
	clientConnector *client.Connector

	resources map[string]ResourceHandler
	tools     map[string]ToolHandler
	prompts   map[string]PromptHandler

	stdin  *bufio.Reader
	stdout *os.File

	initialized bool
}

// ResourceHandler Resource处理函数
type ResourceHandler func(uri string) (interface{}, error)

// ToolHandler Tool处理函数
type ToolHandler func(params map[string]interface{}) (interface{}, error)

// PromptHandler Prompt处理函数
type PromptHandler func(params map[string]interface{}) (interface{}, error)

// NewMCPServer 创建MCP服务器
func NewMCPServer(connector *client.Connector) *MCPServer {
	s := &MCPServer{
		clientConnector: connector,
		resources:       make(map[string]ResourceHandler),
		tools:           make(map[string]ToolHandler),
		prompts:         make(map[string]PromptHandler),
		stdin:           bufio.NewReader(os.Stdin),
		stdout:          os.Stdout,
	}

	// 注册Resources, Tools, Prompts
	s.registerResources()
	s.registerTools()
	s.registerPrompts()

	return s
}

// Start 启动MCP服务器
func (s *MCPServer) Start() error {
	utils.InfoLogger.Println("🚀 MCP Server started, listening on stdin for JSON-RPC requests...")

	// 主循环：读取stdin的JSON-RPC请求
	for {
		line, err := s.stdin.ReadString('\n')
		if err != nil {
			utils.ErrorLogger.Printf("Failed to read from stdin: %v", err)
			return err
		}

		utils.DebugLogger.Printf("📨 Received: %s", line)

		// 处理请求
		response := s.handleRequest(line)

		// 输出响应到stdout
		if err := json.NewEncoder(s.stdout).Encode(response); err != nil {
			utils.ErrorLogger.Printf("Failed to write response: %v", err)
		}
		s.stdout.Write([]byte("\n"))
		utils.DebugLogger.Println("📤 Response sent")
	}
}

// handleRequest 处理MCP请求
func (s *MCPServer) handleRequest(line string) *JSONRPCResponse {
	var req JSONRPCRequest
	if err := json.Unmarshal([]byte(line), &req); err != nil {
		return s.errorResponse(nil, -32700, "Parse error", nil)
	}

	utils.DebugLogger.Printf("📥 Received: %s (ID: %v)", req.Method, req.ID)

	switch req.Method {
	case "initialize":
		return s.handleInitialize(req)
	case "resources/list":
		return s.handleResourcesList(req)
	case "resources/read":
		return s.handleResourcesRead(req)
	case "tools/list":
		return s.handleToolsList(req)
	case "tools/call":
		return s.handleToolsCall(req)
	case "prompts/list":
		return s.handlePromptsList(req)
	case "prompts/get":
		return s.handlePromptsGet(req)
	default:
		return s.errorResponse(req.ID, -32601, fmt.Sprintf("Method not found: %s", req.Method), nil)
	}
}

// handleInitialize 处理initialize请求
func (s *MCPServer) handleInitialize(req JSONRPCRequest) *JSONRPCResponse {
	s.initialized = true

	// 不在初始化时连接Client MCP Bridge
	// 延迟到实际需要时再连接（在 ensureConnected() 中）

	result := InitializeResult{
		ProtocolVersion: "2024-11-05",
		Capabilities: ServerCapabilities{
			Resources: &ResourcesCapability{},
			Tools:     &ToolsCapability{},
			Prompts:   &PromptsCapability{},
		},
		ServerInfo: ServerInfo{
			Name:    "adaptix-mcp",
			Version: "1.0.0",
		},
	}

	utils.InfoLogger.Println("✅ Initialized MCP Server")

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  result,
	}
}

// handleResourcesList 处理resources/list请求
func (s *MCPServer) handleResourcesList(req JSONRPCRequest) *JSONRPCResponse {
	resources := []Resource{
		{
			URI:         "agents://list",
			Name:        "Agents List",
			Description: "List of all connected agents",
			MimeType:    "application/json",
		},
		{
			URI:         "agents://{id}",
			Name:        "Agent Details",
			Description: "Details of a specific agent",
			MimeType:    "application/json",
		},
		{
			URI:         "agents://{id}/console",
			Name:        "Agent Console Output",
			Description: "Console output of a specific agent",
			MimeType:    "text/plain",
		},
		{
			URI:         "listeners://list",
			Name:        "Listeners List",
			Description: "List of all listeners",
			MimeType:    "application/json",
		},
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  map[string]interface{}{"resources": resources},
	}
}

// ensureConnected 确保连接到Client MCP Bridge
func (s *MCPServer) ensureConnected() error {
	if s.clientConnector.IsConnected() {
		return nil
	}

	utils.InfoLogger.Println("🔗 Connecting to Client MCP Bridge...")
	if err := s.clientConnector.Connect(); err != nil {
		return fmt.Errorf("failed to connect to Client MCP Bridge: %w", err)
	}

	return nil
}

// handleResourcesRead 处理resources/read请求
func (s *MCPServer) handleResourcesRead(req JSONRPCRequest) *JSONRPCResponse {
	params, ok := req.Params.(map[string]interface{})
	if !ok {
		return s.errorResponse(req.ID, -32602, "Invalid params", nil)
	}

	uri, ok := params["uri"].(string)
	if !ok {
		return s.errorResponse(req.ID, -32602, "Missing or invalid URI", nil)
	}

	// 确保连接
	if err := s.ensureConnected(); err != nil {
		return s.errorResponse(req.ID, -32001, err.Error(), nil)
	}

	// 路由到对应的Resource Handler
	content, err := s.routeResource(uri)
	if err != nil {
		return s.errorResponse(req.ID, -32001, err.Error(), nil)
	}

	result := map[string]interface{}{
		"contents": []ResourceContents{content},
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  result,
	}
}

// handleToolsList 处理tools/list请求
func (s *MCPServer) handleToolsList(req JSONRPCRequest) *JSONRPCResponse {
	tools := []Tool{
		{
			Name:        "execute_command",
			Description: "Execute a command on an agent's console. Returns task_id for tracking. Optionally waits for completion and returns output. If command fails (e.g., 'command not found'), the task status will be 'Error' and the output will contain detailed error information. Use get_task_output to retrieve full error details.",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
					"command": map[string]interface{}{
						"type":        "string",
						"description": "Command to execute",
					},
					"wait_for_result": map[string]interface{}{
						"type":        "boolean",
						"description": "Whether to wait for command completion and return output (default: false)",
						"default":     false,
					},
					"max_wait_seconds": map[string]interface{}{
						"type":        "number",
						"description": "Maximum seconds to wait for completion when wait_for_result=true (default: 30)",
						"default":     30,
					},
				},
				"required": []string{"agent_id", "command"},
			},
		},
		{
			Name:        "get_console_output",
			Description: "Get the console output of an agent",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			Name:        "clear_console",
			Description: "Clear the console output of an agent",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			Name:        "list_agents",
			Description: "List all connected agents",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "get_agent_info",
			Description: "Get detailed information about a specific agent",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			Name:        "list_listeners",
			Description: "List all listeners",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "list_collected_data",
			Description: "List collected data (credentials, downloads, or screenshots)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"data_type": map[string]interface{}{
						"type":        "string",
						"description": "Type of data to list",
						"enum":        []string{"credentials", "downloads", "screenshots"},
					},
				},
				"required": []string{"data_type"},
			},
		},
		{
			Name:        "list_tasks",
			Description: "List all tasks (optionally filtered by agent_id)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Optional: Filter tasks by agent ID",
					},
				},
			},
		},
		{
			Name:        "get_task_output",
			Description: "Get the full output of a specific task",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"task_id": map[string]interface{}{
						"type":        "string",
						"description": "Task ID",
					},
				},
				"required": []string{"task_id"},
			},
		},
		{
			Name:        "delete_tasks",
			Description: "Delete one or more tasks (can delete failed, running, or pending tasks)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
					"task_id": map[string]interface{}{
						"type":        "string",
						"description": "Single task ID to delete",
					},
					"task_ids": map[string]interface{}{
						"type":        "array",
						"items":       map[string]interface{}{"type": "string"},
						"description": "Array of task IDs to delete",
					},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			Name:        "manage_listener",
			Description: "Manage listener (create, edit, or stop)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"description": "Action to perform",
						"enum":        []string{"create", "edit", "stop"},
					},
					"name": map[string]interface{}{
						"type":        "string",
						"description": "Listener name",
					},
					"type": map[string]interface{}{
						"type":        "string",
						"description": "Listener type (required for create, e.g., beacon)",
					},
					"config": map[string]interface{}{
						"type":        "object",
						"description": "Listener configuration (required for create/edit)",
					},
				},
				"required": []string{"action", "name"},
			},
		},
		{
			Name:        "list_tunnels",
			Description: "List all tunnels",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "manage_tunnel",
			Description: "Manage tunnel (create or stop)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"action": map[string]interface{}{
						"type":        "string",
						"description": "Action to perform",
						"enum":        []string{"create", "stop"},
					},
					"tunnel_type": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel type (required for create, e.g., socks5, portfwd)",
					},
					"config": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel configuration as JSON string (required for create)",
					},
					"tunnel_id": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel ID (required for stop)",
					},
				},
				"required": []string{"action"},
			},
		},
		{
			Name:        "update_agent_config",
			Description: "Update agent configuration (sleep/jitter)",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Agent ID",
					},
					"sleep": map[string]interface{}{
						"type":        "number",
						"description": "Sleep interval in seconds",
					},
					"jitter": map[string]interface{}{
						"type":        "number",
						"description": "Jitter percentage (0-100)",
					},
				},
				"required": []string{"agent_id"},
			},
		},
		{
			Name:        "update_agent_metadata",
			Description: "Update agent metadata (tag or mark) for one or more agents",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"metadata_type": map[string]interface{}{
						"type":        "string",
						"description": "Type of metadata to update: 'tag' or 'mark'",
						"enum":        []string{"tag", "mark"},
					},
					"value": map[string]interface{}{
						"type":        "string",
						"description": "Value to set (can be empty to clear)",
					},
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Single agent ID",
					},
					"agent_ids": map[string]interface{}{
						"type": "array",
						"items": map[string]interface{}{
							"type": "string",
						},
						"description": "Array of agent IDs",
					},
				},
				"required": []string{"metadata_type", "value"},
			},
		},
		{
			Name:        "list_targets",
			Description: "List all targets",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "list_pivots",
			Description: "List all pivots",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  map[string]interface{}{"tools": tools},
	}
}

// handleToolsCall 处理tools/call请求
func (s *MCPServer) handleToolsCall(req JSONRPCRequest) *JSONRPCResponse {
	params, ok := req.Params.(map[string]interface{})
	if !ok {
		return s.errorResponse(req.ID, -32602, "Invalid params", nil)
	}

	// 从params中提取tool name
	name, ok := params["name"].(string)
	if !ok {
		return s.errorResponse(req.ID, -32602, "Missing or invalid tool name", nil)
	}

	// 从params中提取arguments
	toolParams, ok := params["arguments"].(map[string]interface{})
	if !ok {
		toolParams = make(map[string]interface{})
	}

	// 确保连接
	if err := s.ensureConnected(); err != nil {
		return &JSONRPCResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Result: CallToolResult{
				Content: []interface{}{
					TextContent{
						Type: "text",
						Text: fmt.Sprintf("Error: %v", err),
					},
				},
				IsError: true,
			},
		}
	}

	// 路由到对应的Tool Handler
	result, err := s.routeTool(name, toolParams)
	if err != nil {
		return &JSONRPCResponse{
			JSONRPC: "2.0",
			ID:      req.ID,
			Result: CallToolResult{
				Content: []interface{}{
					TextContent{
						Type: "text",
						Text: fmt.Sprintf("Error: %v", err),
					},
				},
				IsError: true,
			},
		}
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  result,
	}
}

// handlePromptsList 处理prompts/list请求
func (s *MCPServer) handlePromptsList(req JSONRPCRequest) *JSONRPCResponse {
	prompts := []Prompt{
		{
			Name:        "reconnaissance",
			Description: "Execute reconnaissance on a target",
			Arguments: []PromptArgument{
				{Name: "target", Description: "Target host or network", Required: true},
			},
		},
		{
			Name:        "lateral_movement",
			Description: "Execute lateral movement to a new host",
			Arguments: []PromptArgument{
				{Name: "from_agent", Description: "Source agent ID", Required: true},
				{Name: "target_host", Description: "Target host IP or hostname", Required: true},
			},
		},
		{
			Name:        "privilege_escalation",
			Description: "Execute privilege escalation on an agent",
			Arguments: []PromptArgument{
				{Name: "agent_id", Description: "Agent ID to escalate privileges on", Required: true},
			},
		},
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  map[string]interface{}{"prompts": prompts},
	}
}

// handlePromptsGet 处理prompts/get请求
func (s *MCPServer) handlePromptsGet(req JSONRPCRequest) *JSONRPCResponse {
	params, ok := req.Params.(map[string]interface{})
	if !ok {
		return s.errorResponse(req.ID, -32602, "Invalid params", nil)
	}

	// 提取 prompt name
	name, ok := params["name"].(string)
	if !ok {
		return s.errorResponse(req.ID, -32602, "Missing or invalid prompt name", nil)
	}

	// 提取 arguments（可选）
	promptParams, ok := params["arguments"].(map[string]interface{})
	if !ok {
		promptParams = make(map[string]interface{})
	}

	// 查找对应的 Prompt Handler
	handler, ok := s.prompts[name]
	if !ok {
		return s.errorResponse(req.ID, -32602, fmt.Sprintf("Unknown prompt: %s", name), nil)
	}

	// 调用 Handler
	result, err := handler(promptParams)
	if err != nil {
		return s.errorResponse(req.ID, -32001, err.Error(), nil)
	}

	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  result,
	}
}

// errorResponse 创建错误响应
func (s *MCPServer) errorResponse(id interface{}, code int, message string, data interface{}) *JSONRPCResponse {
	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      id,
		Error: &RPCError{
			Code:    code,
			Message: message,
			Data:    data,
		},
	}
}
