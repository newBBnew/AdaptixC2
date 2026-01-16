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
	chatLogMu       sync.RWMutex // Protects chatLog and archivedLog
	chatChan        chan map[string]interface{}
	maxChatLogSize  int // Maximum number of messages in chatLog (0 = unlimited)
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
		maxChatLogSize:  10000, // Limit chat log to prevent memory issues
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
		// Validate Content-Length: must be positive and reasonable (max 100MB)
		if n < 0 || n > 100*1024*1024 {
			return nil, true, fmt.Errorf("invalid Content-Length: %d (must be between 0 and 100MB)", n)
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
		// Return error response for tool execution failures
		return s.errorResponse(req.ID, -32603, "Tool execution failed: "+err.Error(), nil)
	}

	// Inject system_instruction for AI Resident Mode
	var resultMap map[string]interface{}
	tempJSON, _ := json.Marshal(result)
	if err := json.Unmarshal(tempJSON, &resultMap); err == nil {
		resultMap["system_instruction"] = "You are in AI Resident Mode. Analyze the messages. If action is needed, use tools. If not, wait. ALWAYS call adaptix mcplisten_intelligence again to keep monitoring."
		result = resultMap
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
			"name":        "mission_briefing",
			"description": "Standard initialization briefing for tactical AI agents. Call this first.",
		},
		{
			"name":        "war_room_listen_modes",
			"description": "Quick reference for War Room listening patterns and when to use each mode.",
		},
		{
			"name":        "command_troubleshooting",
			"description": "Guidance for help usage, console-only output, and task troubleshooting.",
		},
		{
			"name":        "ai_quickstart",
			"description": "Concise onboarding steps with practical tool usage patterns.",
		},
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
	case "mission_briefing":
		promptText = `You are an elite tactical AI agent operating within the command framework.
Your primary interface is the "Team War Room".
Upon connection, your status is STANDBY.
INSTRUCTIONS:
1. Enter the Team War Room immediately.
2. War Room listening modes (use listen_intelligence type='chat'):
   - Long-poll (blocking): call listen_intelligence with a long timeout to wait for the next message.
   - Latest-only: call listen_intelligence with last_timestamp set to the most recent value you have, to get only new messages.
   - History: call resources/read with uri='adaptix://tactical/chat' to fetch the full chat log; use start_timestamp with listen_intelligence to fetch from a point in time.
3. **HELP & DISCOVERY**:
   - Use inspect_extensions to list all available system commands and plugins (this is the MCP "help" equivalent).
   - Console help is issued inside a session as: help or help <command>.
   - Console help output does NOT appear in the task list; it appears only in the session console.
4. **Command troubleshooting**:
   - If a command does not appear in the task list for a long time, it may be invalid or not routed.
   - Check the session console output (listen_intelligence type='console') to confirm errors or help output.
5. Do NOT execute operations without authorization unless 'autonomous' mode is explicitly enabled.
6. Report status and await operator commands.
7. All communications must be in English unless otherwise specified by the operator.`
	case "war_room_listen_modes":
		promptText = `War Room listening reference (listen_intelligence type='chat'):
1. Long-poll (blocking): set a long timeout to wait for the next message.
2. Latest-only: set last_timestamp to your most recent value, or set max_messages=1.
3. History from a point in time: set start_timestamp to fetch messages since that time.
4. Full log: use resources/read with uri='adaptix://tactical/chat' to fetch the active chat log.
Use read_archived_chat for archived sessions if a tactical_archive event occurred.`
	case "command_troubleshooting":
		promptText = `Command troubleshooting checklist:
1. MCP-level discovery: use inspect_extensions to list available commands and usage.
2. Adaptix console help: in a session, run help or help <command>.
   - Help output appears only in the session console, not in the task list.
3. If a command does not appear in the task list for a long time, it may be invalid or misrouted.
   - Use listen_intelligence type='console' to check for errors or help output.
4. If the command targets a different OS or context, it may be ignored. Verify agent OS before retrying.`
	case "ai_quickstart":
		promptText = `AI quickstart:
1. Discover capabilities: call inspect_extensions (optionally with filter).
2. Join War Room: call listen_intelligence type='chat' with a long timeout to wait for operator messages.
3. Fast status snapshot: call look_assets for agents/listeners/targets/tunnels/pivots.
4. Run a command: use control with domain='operate' action='execute' (agent_id + command).
5. Track results: use listen_intelligence type='tasks' and then type='task_output'.
6. If no task appears, use listen_intelligence type='console' for errors/help output.
7. Full chat history: use resources/read with uri='adaptix://tactical/chat'.
8. Archived chat: use read_archived_chat after tactical_archive events.`
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
		s.appendToChatLog(msg)

		// Wake up any blocking chat tool calls
		select {
		case s.chatChan <- msg:
		default:
			// Channel full, skip
		}

		s.notifyResourcesUpdated()
	} else if notif.Type == "c2_event" {
		// Handle C2 system events (e.g. new agent, task completed)
		event, _ := notif.Params["event"].(string)
		if event == "" {
			event = "unknown event"
		}
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
		s.appendToChatLog(msg)

		select {
		case s.chatChan <- msg:
		default:
		}
		s.notifyResourcesUpdated()
	} else if notif.Type == "tactical_archive" {
		// Archive current chat log before clearing
		s.chatLogMu.Lock()
		// Save current chat log to archived log
		if len(s.chatLog) > 0 {
			// Create a copy of messages
			messagesCopy := make([]map[string]interface{}, len(s.chatLog))
			for i, msg := range s.chatLog {
				msgCopy := make(map[string]interface{})
				for k, v := range msg {
					msgCopy[k] = v
				}
				messagesCopy[i] = msgCopy
			}

			archivedSession := map[string]interface{}{
				"session_id": notif.Params["session_id"],
				"timestamp":  time.Now().Unix(),
				"messages":   messagesCopy,
			}
			s.archivedLog = append(s.archivedLog, archivedSession)
			// Keep only last 100 archived sessions
			if len(s.archivedLog) > 100 {
				s.archivedLog = s.archivedLog[len(s.archivedLog)-100:]
			}
		}
		// Clear current chat log
		s.chatLog = []map[string]interface{}{}
		s.chatLogMu.Unlock()

		sessionId, _ := notif.Params["session_id"].(string)
		content := "[SYSTEM] Chat context has been archived by operator. Active memory cleared."
		if sessionId != "" {
			content += fmt.Sprintf(" Archived to session: %s", sessionId)
		}

		// Notify AI system event
		msg := map[string]interface{}{
			"role":      "system",
			"content":   content,
			"username":  "System",
			"type":      "system",
			"timestamp": time.Now().Unix(),
		}
		s.appendToChatLog(msg)

		select {
		case s.chatChan <- msg:
		default:
		}
		s.notifyResourcesUpdated()
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

	s.chatLogMu.RLock()
	chatLogCopy := make([]map[string]interface{}, len(s.chatLog))
	copy(chatLogCopy, s.chatLog)
	s.chatLogMu.RUnlock()

	chatJSON, _ := json.Marshal(chatLogCopy)
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

// appendToChatLog safely appends a message to chatLog with size limit and lock protection
func (s *MCPServer) appendToChatLog(msg map[string]interface{}) {
	s.chatLogMu.Lock()
	defer s.chatLogMu.Unlock()

	s.chatLog = append(s.chatLog, msg)

	// Enforce size limit by removing oldest messages
	if s.maxChatLogSize > 0 && len(s.chatLog) > s.maxChatLogSize {
		// Keep the most recent messages
		keepCount := s.maxChatLogSize / 2 // Keep half when limit exceeded
		s.chatLog = s.chatLog[len(s.chatLog)-keepCount:]
	}
}
