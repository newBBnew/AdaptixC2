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
	utils.InfoLogger.Println("🚀 Starting MCP Server...")
	utils.InfoLogger.Println("📖 Reading from stdin, writing to stdout...")
	utils.InfoLogger.Println("💡 Waiting for JSON-RPC requests...")
	utils.InfoLogger.Println("✅ MCP Server started, listening on stdin...")

	// 主循环：读取stdin的JSON-RPC请求
	for {
		utils.DebugLogger.Println("⏳ Waiting for next request from stdin...")
		line, err := s.stdin.ReadString('\n')
		if err != nil {
			utils.ErrorLogger.Printf("Failed to read from stdin: %v", err)
			return err
		}

		utils.DebugLogger.Printf("📨 Raw input: %s", line)

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
			Description: "Execute a command on an agent's console",
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
			Name:        "list_credentials",
			Description: "List all collected credentials",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "list_downloads",
			Description: "List all file downloads",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
			},
		},
		{
			Name:        "list_screenshots",
			Description: "List all screenshots",
			InputSchema: map[string]interface{}{
				"type":       "object",
				"properties": map[string]interface{}{},
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
			Name:        "create_listener",
			Description: "Create a new listener",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"name": map[string]interface{}{
						"type":        "string",
						"description": "Listener name",
					},
					"type": map[string]interface{}{
						"type":        "string",
						"description": "Listener type (e.g., beacon)",
					},
					"config": map[string]interface{}{
						"type":        "object",
						"description": "Listener configuration",
					},
				},
				"required": []string{"name", "type", "config"},
			},
		},
		{
			Name:        "edit_listener",
			Description: "Edit listener configuration",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"name": map[string]interface{}{
						"type":        "string",
						"description": "Listener name",
					},
					"config": map[string]interface{}{
						"type":        "object",
						"description": "Updated listener configuration",
					},
				},
				"required": []string{"name", "config"},
			},
		},
		{
			Name:        "stop_listener",
			Description: "Stop a listener",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"name": map[string]interface{}{
						"type":        "string",
						"description": "Listener name",
					},
				},
				"required": []string{"name"},
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
			Name:        "create_tunnel",
			Description: "Create a new tunnel",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"tunnel_type": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel type (e.g., socks5, portfwd)",
					},
					"config": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel configuration as JSON string",
					},
				},
				"required": []string{"tunnel_type", "config"},
			},
		},
		{
			Name:        "stop_tunnel",
			Description: "Stop a tunnel",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"tunnel_id": map[string]interface{}{
						"type":        "string",
						"description": "Tunnel ID",
					},
				},
				"required": []string{"tunnel_id"},
			},
		},
		{
			Name:        "remove_agent",
			Description: "Delete one or more agents",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{
						"type":        "string",
						"description": "Single agent ID to remove",
					},
					"agent_ids": map[string]interface{}{
						"type": "array",
						"items": map[string]interface{}{
							"type": "string",
						},
						"description": "Array of agent IDs to remove",
					},
				},
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
			Name:        "set_agent_tag",
			Description: "Set agent tag for one or more agents",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
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
					"tag": map[string]interface{}{
						"type":        "string",
						"description": "Tag to set (can be empty to clear tag)",
					},
				},
				"required": []string{"tag"},
			},
		},
		{
			Name:        "set_agent_mark",
			Description: "Set agent mark for one or more agents",
			InputSchema: map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
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
					"mark": map[string]interface{}{
						"type":        "string",
						"description": "Mark to set (can be empty to clear mark)",
					},
				},
				"required": []string{"mark"},
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
	// TODO: Implement prompts get
	return &JSONRPCResponse{
		JSONRPC: "2.0",
		ID:      req.ID,
		Result:  GetPromptResult{Messages: []interface{}{}},
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
