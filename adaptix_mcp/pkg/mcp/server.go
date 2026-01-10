package mcp

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

type MCPServer struct {
	clientConnector *client.Connector
	tools           map[string]ToolHandler
	stdin           *bufio.Reader
	stdout          *os.File
	writeMu         sync.Mutex // Protects stdout writes
	useFraming      bool       // Tracks if the current session uses HTTP-style framing
	framingMu       sync.RWMutex
	initialized     bool
	chatLog         []map[string]interface{}
	archivedLog     []map[string]interface{}
	chatChan        chan map[string]interface{}
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
		chatLog:         make([]map[string]interface{}, 0),
		archivedLog:     make([]map[string]interface{}, 0),
		chatChan:        make(chan map[string]interface{}, 100),
	}
	s.registerTools()

	// Set notification callback
	connector.SetNotificationCallback(s.handleClientNotification)

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
	s.writeMu.Lock()
	defer s.writeMu.Unlock()

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

		// Update framing state based on the latest request
		s.framingMu.Lock()
		s.useFraming = framed
		s.framingMu.Unlock()

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
	case "prompts/list":
		return s.handlePromptsList(req)
	case "prompts/get":
		return s.handlePromptsGet(req)
	case "resources/list":
		return s.handleResourcesList(req)
	case "resources/read":
		return s.handleResourcesRead(req)
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
			"tools":     map[string]interface{}{},
			"resources": map[string]interface{}{},
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

func (s *MCPServer) handlePromptsList(req JSONRPCRequest) *JSONRPCResponse {
	prompts := []map[string]interface{}{
		{
			"name":        "setup_socks5",
			"description": "Quickly set up a SOCKS5 tunnel on a specified agent",
			"arguments": []map[string]interface{}{
				{"name": "agent_id", "description": "Target agent ID", "required": true},
				{"name": "port", "description": "Local listen port (default 1080)", "required": false},
			},
		},
		{
			"name":        "cleanup_all_ptys",
			"description": "List and close all active PTY sessions",
		},
		{
			"name":        "system_triage",
			"description": "Basic triage for a newly online agent (whoami, ipconfig, tasklist)",
			"arguments": []map[string]interface{}{
				{"name": "agent_id", "description": "Target agent ID", "required": true},
			},
		},
	}
	return s.successResponse(req.ID, map[string]interface{}{"prompts": prompts})
}

func (s *MCPServer) handlePromptsGet(req JSONRPCRequest) *JSONRPCResponse {
	name, _ := req.Params["name"].(string)
	args, _ := req.Params["arguments"].(map[string]interface{})

	var promptText string
	switch name {
	case "setup_socks5":
		port := "1080"
		if p, ok := args["port"].(string); ok {
			port = p
		}
		promptText = fmt.Sprintf("Start a SOCKS5 tunnel on agent %s with listen port %s. Use the manage_tunnel tool. Reference parameters: action='start', type='socks5', data={'agent_id': '%s', 'l_host': '0.0.0.0', 'l_port': %s, 'listen': true}.", args["agent_id"], port, args["agent_id"], port)
	case "cleanup_all_ptys":
		promptText = "First call manage_pty with action='list' to fetch all sessions, then for each returned pty_id call manage_pty with action='close' to clean them up."
	case "system_triage":
		promptText = fmt.Sprintf("On agent %s, execute whoami, ipconfig /all, and tasklist in order, then summarize the results.", args["agent_id"])
	default:
		return s.errorResponse(req.ID, -32602, "Prompt not found", nil)
	}

	result := map[string]interface{}{
		"messages": []map[string]interface{}{
			{
				"role": "user",
				"content": map[string]interface{}{
					"type": "text",
					"text": promptText,
				},
			},
		},
	}
	return s.successResponse(req.ID, result)
}

func (s *MCPServer) handleClientNotification(notif client.Notification) {
	if notif.Type == "tactical_chat" || notif.Type == "team_chat" {
		content, _ := notif.Params["content"].(string)
		username, _ := notif.Params["username"].(string)

		msg := map[string]interface{}{
			"role":      "user",
			"content":   content,
			"username":  username,
			"type":      notif.Type,
			"timestamp": time.Now().Unix(),
		}
		s.chatLog = append(s.chatLog, msg)

		// Wake up any blocking chat tool calls
		select {
		case s.chatChan <- msg:
		default:
			// Channel full, skip
		}

		s.notifyResourcesUpdated()
	} else if notif.Type == "c2_event" {
		// Handle C2 system events (e.g. new agent, task completed)
		event := notif.Params["event"].(string)
		// Format as a system message
		content := fmt.Sprintf("[SYSTEM EVENT] %s", event)
		if agentId, ok := notif.Params["agent_id"].(string); ok {
			content += fmt.Sprintf(" - Agent: %s", agentId)
		}
		if status, ok := notif.Params["status"].(string); ok {
			content += fmt.Sprintf(" - Status: %s", status)
		}

		msg := map[string]interface{}{
			"role":      "system",
			"content":   content,
			"username":  "System",
			"type":      "event",
			"timestamp": time.Now().Unix(),
			"raw_event": notif.Params,
		}
		s.chatLog = append(s.chatLog, msg)

		select {
		case s.chatChan <- msg:
		default:
		}
		s.notifyResourcesUpdated()
	} else if notif.Type == "tactical_archive" {
		// Archive current chat log
		if len(s.chatLog) > 0 {
			archiveEntry := map[string]interface{}{
				"timestamp": time.Now().Unix(),
				"messages":  s.chatLog,
			}
			s.archivedLog = append(s.archivedLog, archiveEntry)
			s.chatLog = []map[string]interface{}{} // Clear active log

			// Notify AI system event
			msg := map[string]interface{}{
				"role":      "system",
				"content":   "[SYSTEM] Chat context has been archived by operator. Active memory cleared.",
				"username":  "System",
				"type":      "system",
				"timestamp": time.Now().Unix(),
			}
			s.chatLog = append(s.chatLog, msg)

			select {
			case s.chatChan <- msg:
			default:
			}
			s.notifyResourcesUpdated()
		}
	}
}

func (s *MCPServer) notifyResourcesUpdated() {
	// Send notification to IDE
	notification := map[string]interface{}{
		"jsonrpc": "2.0",
		"method":  "notifications/resources/updated",
		"params": map[string]interface{}{
			"uri": "adaptix://tactical/chat",
		},
	}
	data, _ := json.Marshal(notification)

	s.framingMu.RLock()
	framed := s.useFraming
	s.framingMu.RUnlock()

	s.writeMu.Lock()
	if framed {
		fmt.Fprintf(s.stdout, "Content-Length: %d\r\n\r\n%s", len(data), data)
	} else {
		s.stdout.Write(data)
		s.stdout.Write([]byte("\n"))
	}
	s.writeMu.Unlock()
}

func (s *MCPServer) handleResourcesList(req JSONRPCRequest) *JSONRPCResponse {
	resources := []map[string]interface{}{
		{
			"uri":         "adaptix://tactical/chat",
			"name":        "Tactical Chat Log",
			"description": "Real-time interactive chat between operator and AI",
			"mimeType":    "application/json",
		},
	}
	return s.successResponse(req.ID, map[string]interface{}{"resources": resources})
}

func (s *MCPServer) handleResourcesRead(req JSONRPCRequest) *JSONRPCResponse {
	uri, _ := req.Params["uri"].(string)
	if uri != "adaptix://tactical/chat" {
		return s.errorResponse(req.ID, -32602, "Resource not found", nil)
	}

	chatJSON, _ := json.Marshal(s.chatLog)
	result := map[string]interface{}{
		"contents": []map[string]interface{}{
			{
				"uri":      uri,
				"mimeType": "application/json",
				"text":     string(chatJSON),
			},
		},
	}
	return s.successResponse(req.ID, result)
}

func (s *MCPServer) successResponse(id interface{}, result interface{}) *JSONRPCResponse {
	return &JSONRPCResponse{JSONRPC: "2.0", ID: id, Result: result}
}

func (s *MCPServer) errorResponse(id interface{}, code int, message string, data interface{}) *JSONRPCResponse {
	return &JSONRPCResponse{JSONRPC: "2.0", ID: id, Error: &RPCError{Code: code, Message: message, Data: data}}
}
