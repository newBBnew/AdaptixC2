package mcp

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

type MCPServer struct {
	clientConnector *client.Connector
	tools           map[string]ToolHandler
	stdin           *bufio.Reader
	stdout          *os.File
	initialized     bool
}

type ToolHandler func(params map[string]interface{}) (interface{}, error)

type JSONRPCRequest struct {
	JSONRPC string                 `json:"jsonrpc"`
	ID      interface{}            `json:"id"`
	Method  string                 `json:"method"`
	Params  map[string]interface{} `json:"params,omitempty"`
}

type JSONRPCResponse struct {
	JSONRPC string      `json:"jsonrpc"`
	ID      interface{} `json:"id"`
	Result  interface{} `json:"result,omitempty"`
	Error   *RPCError   `json:"error,omitempty"`
}

type RPCError struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

func NewMCPServer(connector *client.Connector) *MCPServer {
	s := &MCPServer{
		clientConnector: connector,
		tools:           make(map[string]ToolHandler),
		stdin:           bufio.NewReader(os.Stdin),
		stdout:          os.Stdout,
	}
	s.registerTools()
	return s
}

func (s *MCPServer) readMessage() ([]byte, bool, error) {
	line, err := s.stdin.ReadString('\n')
	if err != nil {
		return nil, false, err
	}

	trimmed := strings.TrimSpace(line)
	if strings.HasPrefix(strings.ToLower(trimmed), "content-length:") {
		value := strings.TrimSpace(trimmed[len("content-length:"):])
		n, err := strconv.Atoi(value)
		if err != nil {
			return nil, true, fmt.Errorf("invalid Content-Length: %w", err)
		}

		for {
			h, err := s.stdin.ReadString('\n')
			if err != nil {
				return nil, true, err
			}
			if strings.TrimSpace(h) == "" {
				break
			}
		}

		payload := make([]byte, n)
		if _, err := io.ReadFull(s.stdin, payload); err != nil {
			return nil, true, err
		}

		return payload, true, nil
	}

	return []byte(trimmed), false, nil
}

func (s *MCPServer) writeMessage(resp *JSONRPCResponse, framed bool) error {
	data, err := json.Marshal(resp)
	if err != nil {
		return err
	}

	if framed {
		if _, err := fmt.Fprintf(s.stdout, "Content-Length: %d\r\n\r\n", len(data)); err != nil {
			return err
		}
		_, err = s.stdout.Write(data)
		return err
	}

	if _, err := s.stdout.Write(data); err != nil {
		return err
	}
	_, err = s.stdout.Write([]byte("\n"))
	return err
}

func (s *MCPServer) Start() error {
	utils.InfoLogger.Println("🚀 MCP Server started, listening on stdin...")

	for {
		payload, framed, err := s.readMessage()
		if err != nil {
			utils.ErrorLogger.Printf("Failed to read from stdin: %v", err)
			return err
		}

		response := s.handleRequest(string(payload))

		if err := s.writeMessage(response, framed); err != nil {
			utils.ErrorLogger.Printf("Failed to write response: %v", err)
		}
	}
}

func (s *MCPServer) handleRequest(line string) *JSONRPCResponse {
	var req JSONRPCRequest
	if err := json.Unmarshal([]byte(line), &req); err != nil {
		return s.errorResponse(nil, -32700, "Parse error", nil)
	}

	switch req.Method {
	case "initialize":
		return s.handleInitialize(req)
	case "tools/list":
		return s.handleToolsList(req)
	case "tools/call":
		return s.handleToolsCall(req)
	case "notifications/initialized":
		return nil
	default:
		return s.errorResponse(req.ID, -32601, "Method not found", nil)
	}
}

func (s *MCPServer) handleInitialize(req JSONRPCRequest) *JSONRPCResponse {
	s.initialized = true

	if err := s.clientConnector.Connect(); err != nil {
		utils.WarnLogger.Printf("Failed to connect to Client: %v", err)
	}

	result := map[string]interface{}{
		"protocolVersion": "2024-11-05",
		"capabilities": map[string]interface{}{
			"tools": map[string]interface{}{},
		},
		"serverInfo": map[string]interface{}{
			"name":    "adaptix-mcp",
			"version": "1.0.0",
		},
	}
	return s.successResponse(req.ID, result)
}

func (s *MCPServer) handleToolsList(req JSONRPCRequest) *JSONRPCResponse {
	tools := s.getToolDefinitions()
	return s.successResponse(req.ID, map[string]interface{}{"tools": tools})
}

func (s *MCPServer) handleToolsCall(req JSONRPCRequest) *JSONRPCResponse {
	name, _ := req.Params["name"].(string)
	args, _ := req.Params["arguments"].(map[string]interface{})

	handler, ok := s.tools[name]
	if !ok {
		return s.errorResponse(req.ID, -32602, "Unknown tool: "+name, nil)
	}

	result, err := handler(args)
	if err != nil {
		return s.successResponse(req.ID, map[string]interface{}{
			"content": []map[string]interface{}{
				{"type": "text", "text": "Error: " + err.Error()},
			},
		})
	}

	text, _ := json.MarshalIndent(result, "", "  ")
	return s.successResponse(req.ID, map[string]interface{}{
		"content": []map[string]interface{}{
			{"type": "text", "text": string(text)},
		},
	})
}

func (s *MCPServer) successResponse(id interface{}, result interface{}) *JSONRPCResponse {
	return &JSONRPCResponse{JSONRPC: "2.0", ID: id, Result: result}
}

func (s *MCPServer) errorResponse(id interface{}, code int, message string, data interface{}) *JSONRPCResponse {
	return &JSONRPCResponse{JSONRPC: "2.0", ID: id, Error: &RPCError{Code: code, Message: message, Data: data}}
}
