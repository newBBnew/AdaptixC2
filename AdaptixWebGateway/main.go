package main

import (
	"bytes"
	"crypto/tls"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf16"

	"AdaptixWebGateway/ui"

	"github.com/dop251/goja"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

var gatewayDebugMode bool

const (
	TYPE_SYNC_BATCH             = 0x14
	TYPE_SYNC_CATEGORY_BATCH    = 0x15
	TYPE_LISTENER_REG           = 0x31
	TYPE_LISTENER_START         = 0x32
	TYPE_LISTENER_STOP          = 0x33
	TYPE_LISTENER_EDIT          = 0x34
	TYPE_AGENT_REG              = 0x41
	TYPE_AGENT_NEW              = 0x42
	TYPE_AGENT_TICK             = 0x43
	TYPE_AGENT_UPDATE           = 0x44
	TYPE_AGENT_REMOVE           = 0x46
	TYPE_AGENT_TASK_UPDATE      = 0x4a
	TYPE_AGENT_CONSOLE_OUT      = 0x69
	TYPE_AGENT_CONSOLE_TASK_UPD = 0x6b
	TYPE_CREDS_CREATE           = 0x81
	TYPE_CREDS_EDIT             = 0x82
	TYPE_CREDS_DELETE           = 0x83
	TYPE_TARGETS_CREATE         = 0x87
	TYPE_TARGETS_EDIT           = 0x88
	TYPE_TARGETS_DELETE         = 0x89
	TYPE_AGENT_TASK_HOOK        = 0x4d
)

// Helper to get int from map[string]interface{} regardless of float64 or json.Number
func getPktInt(pkt map[string]interface{}, key string) int {
	val, ok := pkt[key]
	if !ok {
		return 0
	}
	switch v := val.(type) {
	case float64:
		return int(v)
	case json.Number:
		i, _ := v.Int64()
		return int(i)
	case int64:
		return int(v)
	case int:
		return v
	}
	return 0
}

// Helper to get string from map[string]interface{}
func getPktString(pkt map[string]interface{}, key string) string {
	val, ok := pkt[key]
	if !ok {
		return ""
	}
	if s, ok := val.(string); ok {
		return s
	}
	return fmt.Sprint(val)
}

func processSinglePacket(pkt map[string]interface{}) {
	pktType := getPktInt(pkt, "type")
	if pktType == 0 {
		return
	}

	// Recursively handle batches
	if pktType == TYPE_SYNC_BATCH || pktType == TYPE_SYNC_CATEGORY_BATCH {
		if packets, ok := pkt["packets"].([]interface{}); ok {
			for _, p := range packets {
				if pMap, ok := p.(map[string]interface{}); ok {
					processSinglePacket(pMap)
				}
			}
		}
		return
	}

	// Intercept Task Updates for Post-Handlers (Qt client style)
	if pktType == TYPE_AGENT_TASK_UPDATE || pktType == TYPE_AGENT_CONSOLE_TASK_UPD {
		completed, _ := pkt["a_completed"].(bool)
		handlerId := getPktString(pkt, "a_handler_id")
		if completed && handlerId != "" {
			state.mu.RLock()
			handlerFn, exists := state.Handlers[handlerId]
			runtime := state.Runtime
			state.mu.RUnlock()

			if exists && handlerFn != nil {
				fmt.Printf(" [Gateway] Triggering post-handler: %s\n", handlerId)
				go func() {
					fn, ok := goja.AssertFunction(handlerFn)
					if ok {
						_, err := fn(goja.Undefined(), runtime.ToValue(pkt))
						if err != nil {
							fmt.Printf(" [Gateway] Post-handler error: %v\n", err)
						}
					}
					// Remove handler after use
					state.mu.Lock()
					delete(state.Handlers, handlerId)
					state.mu.Unlock()
				}()
			}
		}
	}

	// Intercept Agent Status for Local State (for ax.agent_info)
	if pktType == 0x42 { // TYPE_AGENT_NEW
		id := getPktString(pkt, "a_id")
		if id != "" {
			state.mu.Lock()
			pkt["a_last_tick"] = float64(time.Now().Unix()) // Set initial tick
			state.ActiveAgents[id] = pkt
			state.mu.Unlock()
		}
	} else if pktType == 0x43 { // TYPE_AGENT_TICK
		if ids, ok := pkt["a_id"].([]interface{}); ok {
			now := float64(time.Now().Unix())
			state.mu.Lock()
			for _, id := range ids {
				idStr := fmt.Sprint(id)
				if agent, exists := state.ActiveAgents[idStr]; exists {
					aMap := agent.(map[string]interface{})
					aMap["a_last_tick"] = now
					state.ActiveAgents[idStr] = aMap
				}
			}
			state.mu.Unlock()
		}
	} else if pktType == 0x44 { // TYPE_AGENT_UPDATE
		id := getPktString(pkt, "a_id")
		if id != "" {
			state.mu.Lock()
			if old, exists := state.ActiveAgents[id]; exists {
				oldMap := old.(map[string]interface{})
				for k, v := range pkt {
					oldMap[k] = v
				}
				state.ActiveAgents[id] = oldMap
			} else {
				state.ActiveAgents[id] = pkt
			}
			state.mu.Unlock()
		}
	} else if pktType == 0x46 { // TYPE_AGENT_REMOVE
		id := getPktString(pkt, "a_id")
		if id != "" {
			state.mu.Lock()
			delete(state.ActiveAgents, id)
			state.mu.Unlock()
		}
	} else if pktType == 0x81 { // TYPE_CREDS_CREATE
		if creds, ok := pkt["c_creds"].([]interface{}); ok {
			state.mu.Lock()
			for _, c := range creds {
				if cMap, ok := c.(map[string]interface{}); ok {
					id := getPktString(cMap, "c_creds_id")
					if id != "" {
						state.Credentials[id] = cMap
					}
				}
			}
			state.mu.Unlock()
		}
	} else if pktType == 0x82 { // TYPE_CREDS_EDIT
		id := getPktString(pkt, "c_creds_id")
		if id != "" {
			state.mu.Lock()
			state.Credentials[id] = pkt
			state.mu.Unlock()
		}
	} else if pktType == 0x83 { // TYPE_CREDS_DELETE
		if ids, ok := pkt["c_creds_id"].([]interface{}); ok {
			state.mu.Lock()
			for _, id := range ids {
				delete(state.Credentials, fmt.Sprint(id))
			}
			state.mu.Unlock()
		}
	} else if pktType == 0x87 { // TYPE_TARGETS_CREATE
		if targets, ok := pkt["t_targets"].([]interface{}); ok {
			state.mu.Lock()
			for _, t := range targets {
				if tMap, ok := t.(map[string]interface{}); ok {
					id := getPktString(tMap, "t_target_id")
					if id != "" {
						state.Targets[id] = tMap
					}
				}
			}
			state.mu.Unlock()
		}
	} else if pktType == 0x88 { // TYPE_TARGETS_EDIT
		id := getPktString(pkt, "t_target_id")
		if id != "" {
			state.mu.Lock()
			state.Targets[id] = pkt
			state.mu.Unlock()
		}
	} else if pktType == 0x89 { // TYPE_TARGETS_DELETE
		if ids, ok := pkt["t_target_id"].([]interface{}); ok {
			state.mu.Lock()
			for _, id := range ids {
				delete(state.Targets, fmt.Sprint(id))
			}
			state.mu.Unlock()
		}
	}

	// Intercept Registration Packets
	if pktType == TYPE_LISTENER_REG {
		axScript := getPktString(pkt, "ax")
		if axScript != "" {
			fmt.Printf("🎨 [Gateway] Transpiling UI for LISTENER_REG...\n")
			// ListenerUI(mode_create=true)
			schema, err := ui.TranspileScript(axScript, "ListenerUI", true)
			if err != nil {
				fmt.Printf("❌ [Gateway] UI Transpilation failed: %v\n", err)
				pkt["ui_error"] = err.Error()
			} else {
				pkt["ui_schema"] = schema
			}
		}
	} else if pktType == TYPE_AGENT_REG {
		axScript := getPktString(pkt, "ax")
		if axScript != "" {
			fmt.Printf("🎨 [Gateway] Transpiling UI for AGENT_REG...\n")

			listenersRaw, _ := pkt["listeners"].([]interface{})
			schemas := make(map[string]*ui.UISchema)

			// Transpile GenerateUI for each supported listener type
			for _, lVal := range listenersRaw {
				lType, ok := lVal.(string)
				if !ok {
					continue
				}

				schema, err := ui.TranspileScript(axScript, "GenerateUI", lType)
				if err == nil {
					schemas[lType] = schema
				} else {
					fmt.Printf("⚠️ [Gateway] Failed to transpile for listener type %s: %v\n", lType, err)
				}
			}

			if len(schemas) > 0 {
				fmt.Printf("✅ [Gateway] UI Transpilation success (Variants: %d)\n", len(schemas))
				pkt["ui_schema"] = schemas
			}
		}
	} else if pktType == TYPE_AGENT_TASK_HOOK {
		hookId := getPktString(pkt, "a_hook_id")
		if hookId != "" {
			state.mu.RLock()
			hookFn, exists := state.PostHooks[hookId]
			runtime := state.Runtime
			state.mu.RUnlock()

			if exists && hookFn != nil {
				fn, ok := goja.AssertFunction(hookFn)
				if ok {
					fmt.Printf("🪝 [Gateway] Triggering post-hook: %s\n", hookId)
					val, err := fn(goja.Undefined(), runtime.ToValue(pkt))
					if err == nil {
						// Post-hooks in Qt can return a modified packet object or null
						if !goja.IsUndefined(val) && !goja.IsNull(val) {
							if modifiedPkt, ok := val.Export().(map[string]interface{}); ok {
								for k, v := range modifiedPkt {
									pkt[k] = v
								}
							}
						}
					} else {
						fmt.Printf("❌ [Gateway] Post-hook error: %v\n", err)
					}
				}
			}
		}
	}
}

func processBackendMessage(msg []byte) []byte {
	// Use json.NewDecoder with UseNumber() to prevent precision loss for Task IDs and large integers
	var jsonObj map[string]interface{}
	decoder := json.NewDecoder(bytes.NewReader(msg))
	decoder.UseNumber()
	if err := decoder.Decode(&jsonObj); err != nil {
		// Not JSON or mixed content, ignore
		return msg
	}

	if t, ok := jsonObj["type"]; ok {
		pktType := 0
		switch v := t.(type) {
		case json.Number:
			i, _ := v.Int64()
			pktType = int(i)
		case float64:
			pktType = int(v)
		case int:
			pktType = v
		case int64:
			pktType = int(v)
		}
		if pktType == 0x6a || pktType == 0x6b {
			aid := getPktString(jsonObj, "a_id")
			cmd := getPktString(jsonObj, "a_cmdline")
			if aid != "" && cmd != "" && strings.HasPrefix(strings.ToLower(strings.TrimSpace(cmd)), "execute bof") {
				state.mu.RLock()
				display := state.AliasDisplayCmd[aid]
				ts := state.AliasDisplayTime[aid]
				state.mu.RUnlock()
				if display != "" && ts > 0 && (time.Now().Unix()-ts) <= 20 {
					jsonObj["a_cmdline"] = display
					state.mu.Lock()
					delete(state.AliasDisplayCmd, aid)
					delete(state.AliasDisplayTime, aid)
					state.mu.Unlock()
				}
			}
		}
	}

	// Minimal debug: only print critical packet types for command output troubleshooting
	// (avoid dumping full payloads)
	if t, ok := jsonObj["type"]; ok {
		pktType := 0
		switch v := t.(type) {
		case json.Number:
			i, _ := v.Int64()
			pktType = int(i)
		case float64:
			pktType = int(v)
		case int:
			pktType = v
		case int64:
			pktType = int(v)
		}
		if pktType == 0x61 || pktType == 0x62 || pktType == 0x64 || pktType == 0x69 || pktType == 0x6a || pktType == 0x6b {
			aid := getPktString(jsonObj, "b_agent_id")
			if aid == "" {
				aid = getPktString(jsonObj, "a_id")
			}
			last := ""
			state.mu.RLock()
			if aid != "" {
				last = state.LastCmd[aid]
			}
			state.mu.RUnlock()
			if last != "" {
				fmt.Printf("📦 [Gateway] Packet type=0x%X agent=%s last_cmd=%s\n", pktType, aid, last)
			} else {
				fmt.Printf("📦 [Gateway] Packet type=0x%X agent=%s\n", pktType, aid)
			}
		}
	}

	processSinglePacket(jsonObj)

	// Re-marshal
	newMsg, err := json.Marshal(jsonObj)
	if err != nil {
		fmt.Printf("❌ [Gateway] Failed to re-marshal packet: %v\n", err)
		return msg
	}
	return newMsg
}

// --- Models ---

type Command struct {
	Name        string              `json:"name"`
	Description string              `json:"description"`
	Example     string              `json:"example"`
	Group       string              `json:"group"`
	ScriptDir   string              `json:"-"` // Store the directory where this command's script is located
	PreHook     goja.Value          `json:"-"` // Not serialized to JSON
	Args        []interface{}       `json:"args"`
	SubCommands map[string]*Command `json:"sub_commands,omitempty"`
}

type Plugin struct {
	Category  string     `json:"category"`
	Command   string     `json:"command"`
	Agents    []string   `json:"agents"`
	OS        []string   `json:"os"`
	Listeners []string   `json:"listeners"`
	Callback  goja.Value `json:"-"`
}

type GatewayState struct {
	mu                sync.RWMutex
	C2Endpoint        string
	IsConnected       bool
	LastToken         string // Store the most recent authorization token for internal script calls
	ActiveAgents      map[string]interface{}
	Credentials       map[string]interface{} // Store synchronized credentials
	Targets           map[string]interface{} // Store synchronized targets
	AliasDisplayCmd   map[string]string
	AliasDisplayTime  map[string]int64
	LastCmd           map[string]string
	LastCmdTime       map[string]int64
	ScriptsPath       string
	CurrentLoadingDir string // Track the directory of the script currently being loaded
	Commands          map[string]*Command
	Plugins           []Plugin
	Handlers          map[string]goja.Value
	PostHooks         map[string]goja.Value
	WebSockets        map[*websocket.Conn]bool
	Runtime           *goja.Runtime
}

var state = &GatewayState{
	C2Endpoint:       "https://127.0.0.1:8443",
	ActiveAgents:     make(map[string]interface{}),
	Credentials:      make(map[string]interface{}),
	Targets:          make(map[string]interface{}),
	AliasDisplayCmd:  make(map[string]string),
	AliasDisplayTime: make(map[string]int64),
	LastCmd:          make(map[string]string),
	LastCmdTime:      make(map[string]int64),
	ScriptsPath:      "./Extension-Kit",
	Commands:         make(map[string]*Command),
	Plugins:          make([]Plugin, 0),
	Handlers:         make(map[string]goja.Value),
	PostHooks:        make(map[string]goja.Value),
	WebSockets:       make(map[*websocket.Conn]bool),
}

func broadcastToWebUI(pkt interface{}) {
	state.mu.RLock()
	defer state.mu.RUnlock()

	msg, err := json.Marshal(pkt)
	if err != nil {
		return
	}

	for ws := range state.WebSockets {
		err := ws.WriteMessage(websocket.TextMessage, msg)
		if err != nil {
			fmt.Printf("⚠️ [Gateway] Failed to broadcast to WS: %v\n", err)
		}
	}
}

// httpRequestToC2 sends a generic HTTP request to C2 with internal authentication
func httpRequestToC2(method, endpoint string, payload interface{}) (int, []byte, error) {
	targetURL := fmt.Sprintf("%s%s", state.C2Endpoint, endpoint)

	var reqBody io.Reader
	if payload != nil {
		jsonPayload, _ := json.Marshal(payload)
		reqBody = strings.NewReader(string(jsonPayload))
	}

	req, err := http.NewRequest(method, targetURL, reqBody)
	if err != nil {
		return 500, nil, err
	}

	if payload != nil {
		req.Header.Set("Content-Type", "application/json")
	}

	state.mu.RLock()
	token := state.LastToken
	state.mu.RUnlock()

	if token != "" {
		req.Header.Set("Authorization", token)
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		},
		Timeout: 10 * time.Second,
	}

	resp, err := client.Do(req)
	if err != nil {
		return 502, nil, err
	}
	defer resp.Body.Close()

	respBody, err := ioutil.ReadAll(resp.Body)
	return resp.StatusCode, respBody, err
}

// httpPostToC2 is a helper for POST requests
func httpPostToC2(endpoint string, payload interface{}) (int, []byte, error) {
	return httpRequestToC2("POST", endpoint, payload)
}

// refreshActiveAgents fetches the current agent list from C2 to populate Gateway state
func refreshActiveAgents() {
	// Use GET as confirmed by C2 source code
	status, body, err := httpRequestToC2("GET", "/agent/list", nil)
	if err != nil || status != 200 {
		fmt.Printf("⚠️ [Gateway] Failed to pre-warm agent state: %v (Status: %d)\n", err, status)
		return
	}

	var agents []map[string]interface{}
	// Use Number for ID precision
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.UseNumber()
	if err := decoder.Decode(&agents); err == nil {
		state.mu.Lock()
		for _, a := range agents {
			id := getPktString(a, "a_id")
			if id != "" {
				state.ActiveAgents[id] = a
			}
		}
		state.mu.Unlock()
		fmt.Printf("🔥 [Gateway] Pre-warmed state with %d agents\n", len(agents))
	} else {
		fmt.Printf("⚠️ [Gateway] Failed to decode agent list: %v\n", err)
	}
}

// findCommand recursively finds a command or subcommand in the tree
func findCommand(tree map[string]*Command, parts []string) (*Command, []string, int) {
	if len(parts) == 0 {
		return nil, nil, 0
	}
	cmd, exists := tree[strings.ToLower(parts[0])]
	if !exists {
		return nil, nil, 0
	}
	if len(parts) > 1 && cmd.SubCommands != nil {
		sub, subRemaining, depth := findCommand(cmd.SubCommands, parts[1:])
		if sub != nil {
			return sub, subRemaining, depth + 1
		}
	}
	return cmd, parts[1:], 1
}

func unserializeParams(commandline string) []string {
	var tokens []string
	var token strings.Builder
	inQuotes := false
	lenStr := len(commandline)

	for i := 0; i < lenStr; {
		c := commandline[i]

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') && !inQuotes {
			if token.Len() > 0 {
				tokens = append(tokens, token.String())
				token.Reset()
			}
			i++
			continue
		}

		if c == '"' {
			inQuotes = !inQuotes
			i++
			continue
		}

		if c == '\\' {
			numBS := 0
			for i < lenStr && commandline[i] == '\\' {
				numBS++
				i++
			}
			if i < lenStr && commandline[i] == '"' {
				token.WriteString(strings.Repeat("\\", numBS/2))
				if numBS%2 == 0 {
					inQuotes = !inQuotes
				} else {
					token.WriteByte('"')
				}
				i++
			} else {
				token.WriteString(strings.Repeat("\\", numBS))
			}
			continue
		}

		token.WriteByte(c)
		i++
	}

	if token.Len() > 0 {
		tokens = append(tokens, token.String())
	}

	return tokens
}

// parseArgs converts cli parts into a map based on command metadata
func parseArgs(cmd *Command, remainingParts []string) map[string]interface{} {
	args := make(map[string]interface{})
	if cmd == nil {
		return args
	}

	// Simple positional and flag-based parser mimicking Qt client behavior
	posIdx := 0
	for i := 0; i < len(remainingParts); i++ {
		part := remainingParts[i]
		foundFlag := false

		// Check if it matches any flag-based argument
		for _, argRaw := range cmd.Args {
			arg, ok := argRaw.(map[string]interface{})
			if !ok {
				continue
			}
			flag, _ := arg["flag"].(string)
			if flag != "" && flag == part {
				foundFlag = true
				name, _ := arg["name"].(string)
				argType, _ := arg["type"].(string)

				if argType == "bool" {
					args[name] = true
				} else if i+1 < len(remainingParts) {
					val := remainingParts[i+1]
					if argType == "int" || argType == "flag_int" {
						var n int
						fmt.Sscanf(val, "%d", &n)
						args[name] = n
					} else {
						args[name] = val
					}
					i++ // Skip value part
				}
				break
			}
		}

		if !foundFlag {
			// Find next positional argument
			for j := posIdx; j < len(cmd.Args); j++ {
				arg, ok := cmd.Args[j].(map[string]interface{})
				if !ok {
					continue
				}
				flag, _ := arg["flag"].(string)
				if flag == "" {
					name, _ := arg["name"].(string)
					args[name] = part
					posIdx = j + 1
					break
				}
			}
		}
	}
	return args
}

func cleanTrailingQuote(s string) string {
	for strings.HasSuffix(s, "\"") {
		s = strings.TrimSuffix(s, "\"")
	}
	return s
}

// forwardCommandToC2 sends a command execution request to the backend C2 server and returns response
func forwardCommandToC2(c *gin.Context, agentID, cmdline, name string, data map[string]interface{}, ui bool, hookId string, handlerId string) (int, []byte, error) {
	targetURL := fmt.Sprintf("%s/agent/command/execute", state.C2Endpoint)

	// DEBUG: Log incoming command request details
	fmt.Printf("🚀 [Gateway] Executing command: Agent=%s, CmdLine=%s, Origin=%s\n", agentID, cmdline, name)
	state.mu.Lock()
	state.LastCmd[agentID] = cmdline
	state.LastCmdTime[agentID] = time.Now().Unix()
	state.mu.Unlock()

	// Resolve the real Agent Name (Type) from state if not provided or generic
	// C2 Server uses this to find the correct extender/config (e.g. "beacon")
	resolvedName := name
	state.mu.RLock()
	agent, exists := state.ActiveAgents[agentID]
	state.mu.RUnlock()

	// CRITICAL: Only resolve if it's a generic "alias" or empty.
	// DO NOT resolve if it's "browser", as that's a special subsystem for structured JSON data.
	if resolvedName == "" || resolvedName == "alias" {
		if exists {
			if aMap, ok := agent.(map[string]interface{}); ok {
				resolvedName = getPktString(aMap, "a_name")
			}
		} else if name != "" && name != "alias" && name != "browser" {
			// Proactively cache if we got a valid name from the request
			state.mu.Lock()
			state.ActiveAgents[agentID] = map[string]interface{}{
				"a_id":   agentID,
				"a_name": name,
			}
			state.mu.Unlock()
			resolvedName = name
			fmt.Printf("💡 [Gateway] Proactively cached Agent %s as type '%s'\n", agentID, name)
		}
	}

	if resolvedName == "" || resolvedName == "alias" {
		state.mu.RLock()
		var keys []string
		for k := range state.ActiveAgents {
			keys = append(keys, k)
		}
		state.mu.RUnlock()
		fmt.Printf("⚠️ [Gateway] Warning: Agent %s name resolution failed or generic ('%s'). C2 might reject this. Available agents in cache: %v\n", agentID, resolvedName, keys)
	}

	// Determine the actual command name from cmdline for internal Gateway logic/logging
	parts := unserializeParams(strings.TrimSpace(cmdline))
	cmdName := ""
	subName := ""
	if len(parts) > 0 {
		cmdName = strings.ToLower(parts[0])
	}
	if len(parts) > 1 {
		candidate := strings.ToLower(parts[1])
		switch cmdName {
		case "execute", "ps", "jobs", "link", "exfil", "profile", "lportfwd", "rportfwd":
			subName = candidate
		}
	}

	// C2 Core Extenders (e.g. beacon_agent) expect 'command' and 'subcommand'
	// to be present INSIDE the 'data' JSON object, not at the top level.
	if data == nil {
		data = make(map[string]interface{})
	}
	if _, exists := data["command"]; !exists && cmdName != "" {
		data["command"] = cmdName
	}
	if _, exists := data["subcommand"]; !exists && subName != "" {
		data["subcommand"] = subName
	}

	// Provide required args for structured commands when Web UI didn't send parsed data
	if cmdName == "ls" {
		if _, exists := data["directory"]; !exists {
			dir := "."
			if len(parts) > 1 {
				dir = cleanTrailingQuote(parts[1])
			}
			data["directory"] = dir
		}
	}
	if cmdName == "cd" {
		if _, exists := data["path"]; !exists {
			if len(parts) > 1 {
				data["path"] = cleanTrailingQuote(parts[1])
			}
		}
	}
	if cmdName == "cat" {
		if _, exists := data["path"]; !exists {
			if len(parts) > 1 {
				data["path"] = cleanTrailingQuote(parts[1])
			}
		}
	}
	if cmdName == "download" {
		if _, exists := data["file"]; !exists {
			if len(parts) > 1 {
				data["file"] = cleanTrailingQuote(parts[1])
			}
		}
	}
	if cmdName == "rm" || cmdName == "mkdir" {
		if _, exists := data["path"]; !exists {
			if len(parts) > 1 {
				data["path"] = cleanTrailingQuote(parts[1])
			}
		}
	}

	// SPECIAL CASE: 'execute bof' requires 'bof_path' in the data map for Web Client mode (Notification Mode)
	if cmdName == "execute" && subName == "bof" && len(parts) >= 3 {
		if _, exists := data["bof_path"]; !exists {
			bofPath := parts[2]
			// If the script provided an absolute path, we MUST make it relative to Extension-Kit
			// because the C2 Extender (pl_agent.go) rejects absolute paths and joins them to its own root.
			absScripts, _ := filepath.Abs(state.ScriptsPath)
			absBof, _ := filepath.Abs(bofPath)

			if strings.HasPrefix(absBof, absScripts) {
				relPath, err := filepath.Rel(absScripts, absBof)
				if err == nil {
					// Ensure we use forward slashes for C2 compatibility
					data["bof_path"] = filepath.ToSlash(relPath)
				} else {
					data["bof_path"] = bofPath
				}
			} else {
				// Fallback: If the script provides a path like "Extension-Kit/SAL-BOF/_bin/arp.x64.o"
				// but we are running in 'release', we need to strip the prefix to make it relative to the C2 root
				parts := strings.Split(filepath.ToSlash(bofPath), "/")
				found := false
				for i, p := range parts {
					if p == "Extension-Kit" && i+1 < len(parts) {
						data["bof_path"] = strings.Join(parts[i+1:], "/")
						found = true
						break
					}
				}
				if !found {
					data["bof_path"] = bofPath
				}
			}
			fmt.Printf("🎯 [Gateway] Injected bof_path: %v\n", data["bof_path"])
		}
	}

	// C2 Core expects 'data' field to be a JSON string
	dataJSON, _ := json.Marshal(data)

	payload := map[string]interface{}{
		"id":            agentID,
		"name":          resolvedName,
		"cmdline":       cmdline,
		"data":          string(dataJSON),
		"ui":            ui,
		"ax_hook_id":    hookId,
		"ax_handler_id": handlerId,
	}

	jsonPayload, _ := json.Marshal(payload)
	fmt.Printf("📤 [Gateway] Forwarding to C2: %s\n", string(jsonPayload))

	req, err := http.NewRequest("POST", targetURL, strings.NewReader(string(jsonPayload)))
	if err != nil {
		fmt.Printf("❌ [Gateway] Failed to create forward request: %v\n", err)
		return 500, []byte(fmt.Sprintf(`{"ok": false, "message": "Gateway error: %v"}`, err)), err
	}

	req.Header.Set("Content-Type", "application/json")

	// Authentication handling
	if c != nil {
		// 1. Prefer token from current request headers
		if auth := c.GetHeader("Authorization"); auth != "" {
			req.Header.Set("Authorization", auth)
		} else if token := c.Query("token"); token != "" {
			req.Header.Set("Authorization", "Bearer "+token)
		} else if mcpToken := c.Query("mcp_token"); mcpToken != "" {
			req.Header.Set("Authorization", "Bearer "+mcpToken)
		}
	} else {
		// 2. Fallback to LastToken for internal calls (from Goja scripts/handlers)
		state.mu.RLock()
		lastToken := state.LastToken
		state.mu.RUnlock()
		if lastToken != "" {
			req.Header.Set("Authorization", lastToken)
		}
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
		},
		Timeout: 30 * time.Second,
	}

	resp, err := client.Do(req)
	if err != nil {
		fmt.Printf("❌ [Gateway] Command forwarding failed: %v\n", err)
		return 502, []byte(fmt.Sprintf(`{"ok": false, "message": "Backend unavailable: %v"}`, err)), err
	}
	defer resp.Body.Close()

	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("❌ [Gateway] Failed to read backend response: %v\n", err)
		return 500, []byte(`{"ok": false, "message": "Failed to read backend response"}`), err
	}

	// C2 often returns HTTP 200 even when ok=false; surface this clearly for debugging.
	if resp.StatusCode == 200 {
		var r struct {
			OK      bool   `json:"ok"`
			Message string `json:"message"`
		}
		if err := json.Unmarshal(respBody, &r); err == nil {
			if !r.OK {
				fmt.Printf("❌ [Gateway] C2 rejected command: agent=%s name=%s cmdline=%s message=%s\n", agentID, resolvedName, cmdline, r.Message)
			}
		}
	}

	if resp.StatusCode != 200 {
		fmt.Printf("⚠️ [Gateway] C2 returned error status %d: %s\n", resp.StatusCode, string(respBody))
	}
	return resp.StatusCode, respBody, nil
}

func main() {
	// --- Flag Parsing ---
	c2Addr := flag.String("c2", "", "C2 Teamserver Endpoint (overrides profile.json)")
	listenAddr := flag.String("listen", ":8080", "Gateway Listen Address")
	scriptsPath := flag.String("scripts", "./Extension-Kit", "Path to Extension-Kit scripts")
	profilePath := flag.String("profile", "./profile.json", "Path to C2 profile.json")
	debug := flag.Bool("debug", false, "Enable debug mode (verbose logging)")
	flag.Parse()

	if !*debug {
		gin.SetMode(gin.ReleaseMode)
	}

	// --- Load Profile ---
	if *c2Addr == "" {
		if _, err := os.Stat(*profilePath); err == nil {
			fmt.Printf("📖 [Gateway] Loading configuration from %s\n", *profilePath)
			content, _ := ioutil.ReadFile(*profilePath)
			var profile struct {
				Teamserver struct {
					Interface string `json:"interface"`
					Port      int    `json:"port"`
					Endpoint  string `json:"endpoint"`
				} `json:"Teamserver"`
			}
			if err := json.Unmarshal(content, &profile); err == nil {
				host := profile.Teamserver.Interface
				if host == "0.0.0.0" {
					host = "127.0.0.1"
				}
				// Use the endpoint prefix from profile if present
				baseEndpoint := strings.TrimSuffix(profile.Teamserver.Endpoint, "/")
				state.C2Endpoint = fmt.Sprintf("https://%s:%d%s", host, profile.Teamserver.Port, baseEndpoint)
				fmt.Printf("✅ [Gateway] Auto-configured from profile: %s\n", state.C2Endpoint)
			}
		}
	} else {
		state.C2Endpoint = *c2Addr
	}

	if state.C2Endpoint == "" {
		state.C2Endpoint = "https://127.0.0.1:8443" // Default fallback
	}

	state.ScriptsPath = *scriptsPath

	r := gin.Default()

	// Security: Don't trust all proxies by default
	_ = r.SetTrustedProxies(nil)

	// --- 2. Security Layer & API Routes ---
	// Define API routes BEFORE NoRoute to avoid conflict
	api := r.Group("/api")
	api.Use(func(c *gin.Context) {
		path := c.Request.URL.Path

		// Extract token from Header or Query (important for WebSocket)
		tokenStr := c.GetHeader("Authorization")
		if tokenStr == "" {
			tokenStr = c.Query("token")
			if tokenStr != "" {
				tokenStr = "Bearer " + tokenStr
			}
		}

		// Allow login, refresh and connect (WebSocket) to bypass gateway-level auth
		// because they will be validated by the Teamserver backend through proxy
		if strings.HasSuffix(path, "/login") ||
			strings.HasSuffix(path, "/refresh") ||
			strings.HasSuffix(path, "/connect") {
			c.Next()
			return
		}

		// Cloudflare Tunnel Auth Support
		cfID := c.GetHeader("CF-Access-Client-Id")
		cfSecret := c.GetHeader("CF-Access-Client-Secret")
		if cfID != "" && cfSecret != "" {
			c.Next()
			return
		}

		if tokenStr == "" && c.Query("mcp_token") == "" {
			fmt.Printf("⚠️ [Gateway] Unauthorized access attempt to %s\n", path)
			c.AbortWithStatusJSON(401, gin.H{"error": "Unauthorized Access", "path": path})
			return
		}

		// Store the token for internal script-initiated C2 calls
		if tokenStr != "" {
			state.mu.Lock()
			needsRefresh := state.LastToken == ""
			state.LastToken = tokenStr
			state.mu.Unlock()
			if needsRefresh {
				fmt.Printf("🔑 [Gateway] First token captured, pre-warming agent state...\n")
				go refreshActiveAgents()
			}
		} else if mcpToken := c.Query("mcp_token"); mcpToken != "" {
			state.mu.Lock()
			needsRefresh := state.LastToken == ""
			state.LastToken = "Bearer " + mcpToken
			state.mu.Unlock()
			if needsRefresh {
				fmt.Printf("🔑 [Gateway] First MCP token captured, pre-warming agent state...\n")
				go refreshActiveAgents()
			}
		}

		c.Next()
	})

	// --- 3. C2 Proxy Layer ---
	targetURL, _ := url.Parse(state.C2Endpoint)
	proxy := httputil.NewSingleHostReverseProxy(targetURL)

	// Refined Director to handle Host header correctly
	originalDirector := proxy.Director
	proxy.Director = func(req *http.Request) {
		originalDirector(req)
		req.Host = targetURL.Host // Critical for TLS and some backends
	}

	proxy.Transport = &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}

	proxy.ModifyResponse = func(resp *http.Response) error {
		if gatewayDebugMode || resp.StatusCode >= 400 {
			fmt.Printf("⬅️ [Gateway-Proxy] Backend Response: %d %s (Request: %s %s)\n",
				resp.StatusCode, resp.Status, resp.Request.Method, resp.Request.URL.Path)
		}
		return nil
	}

	// WebSocket Upgrader
	upgrader := websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool { return true },
	}

	api.Any("/proxy/*path", func(c *gin.Context) {
		path := c.Param("path")

		if gatewayDebugMode {
			fmt.Printf("🔍 [Gateway] Proxying request: %s %s -> %s\n", c.Request.Method, c.Request.URL.Path, path)
		}

		// Handle WebSocket proxying
		if c.GetHeader("Upgrade") == "websocket" {
			targetWS := strings.Replace(state.C2Endpoint, "http", "ws", 1)
			if !strings.HasSuffix(targetWS, "/") && !strings.HasPrefix(path, "/") {
				targetWS += "/"
			}
			targetWS += path
			if c.Request.URL.RawQuery != "" {
				targetWS += "?" + c.Request.URL.RawQuery
			}

			fmt.Printf("📡 [Gateway] WS Dial: %s\n", targetWS)

			// Dial backend
			d := websocket.Dialer{
				TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
			}

			// Pass through critical headers for C2 channels (tunnel, terminal)
			headers := http.Header{}
			for _, h := range []string{"Channel-Type", "Channel-Data", "Authorization"} {
				if val := c.GetHeader(h); val != "" {
					headers.Set(h, val)
				}
			}

			// Browser WebSocket client compatibility: extract from query params if headers missing
			if headers.Get("Channel-Type") == "" {
				if val := c.Query("channel_type"); val != "" {
					headers.Set("Channel-Type", val)
				}
			}
			if headers.Get("Channel-Data") == "" {
				if val := c.Query("channel_data"); val != "" {
					headers.Set("Channel-Data", val)
				}
			}

			// Also support token from query if present
			if token := c.Query("token"); token != "" && headers.Get("Authorization") == "" {
				headers.Set("Authorization", "Bearer "+token)
			}

			backendConn, _, err := d.Dial(targetWS, headers)
			if err != nil {
				fmt.Printf("❌ [Gateway] WS Dial Error: %v\n", err)
				c.AbortWithStatus(502)
				return
			}
			defer backendConn.Close()

			// Upgrade frontend
			clientConn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
			if err != nil {
				fmt.Printf("❌ [Gateway] WS Upgrade Error: %v\n", err)
				return
			}

			// Register WebSocket for broadcasting
			state.mu.Lock()
			state.WebSockets[clientConn] = true
			state.mu.Unlock()

			defer func() {
				clientConn.Close()
				state.mu.Lock()
				delete(state.WebSockets, clientConn)
				state.mu.Unlock()
			}()

			// Bidirectional pipe
			errChan := make(chan error, 2)
			go func() {
				for {
					mt, msg, err := clientConn.ReadMessage()
					if err != nil {
						errChan <- err
						return
					}

					if gatewayDebugMode && mt == websocket.TextMessage {
						fmt.Printf("🖥️ [Web -> Server] %s\n", string(msg))
					}

					err = backendConn.WriteMessage(mt, msg)
					if err != nil {
						errChan <- err
						return
					}
				}
			}()
			go func() {
				for {
					mt, msg, err := backendConn.ReadMessage()
					if err != nil {
						errChan <- err
						return
					}

					// Intercept and process backend messages (UI Transpilation)
					// NOTE: Teamserver sends /connect stream as BinaryMessage containing JSON.
					// Browsers don't consume binary frames as text JSON, so convert for /connect only.
					if mt == websocket.TextMessage {
						if gatewayDebugMode {
							fmt.Printf("📡 [Server -> Web] Type=%d, Size=%d, Payload=%s\n", mt, len(msg), string(msg))
						}
						msg = processBackendMessage(msg)
					} else if mt == websocket.BinaryMessage && strings.Contains(path, "/connect") {
						if gatewayDebugMode {
							fmt.Printf("📡 [Server -> Web] Type=%d, Size=%d (binary-json)\n", mt, len(msg))
						}
						msg = processBackendMessage(msg)
						mt = websocket.TextMessage
					}

					err = clientConn.WriteMessage(mt, msg)
					if err != nil {
						errChan <- err
						return
					}
				}
			}()
			<-errChan
			return
		}

		// Handle Standard HTTP proxying
		// Ensure path has leading slash for the proxy
		if !strings.HasPrefix(path, "/") {
			path = "/" + path
		}
		c.Request.URL.Path = path
		proxy.ServeHTTP(c.Writer, c.Request)
	})

	// --- 4. Extension-Kit APIs ---
	// (Goja engine initialization and metadata routes)

	// Initial load
	state.Runtime = goja.New()
	setupAxAPI(state.Runtime, &state.Commands, &state.Plugins)
	_ = loadMainExtension(state.Runtime)

	// Pre-warm agent state for name resolution in scripts
	go refreshActiveAgents()

	api.GET("/extensions/metadata", func(c *gin.Context) {
		state.mu.RLock()
		defer state.mu.RUnlock()

		// Hide commands that are already registered as sub-commands of other commands.
		hidden := make(map[string]bool)
		for _, cmd := range state.Commands {
			for subName := range cmd.SubCommands {
				hidden[strings.ToLower(subName)] = true
			}
		}
		filtered := make(map[string]*Command)
		for name, cmd := range state.Commands {
			if hidden[strings.ToLower(name)] {
				continue
			}
			filtered[name] = cmd
		}
		c.JSON(200, gin.H{
			"commands": filtered,
			"plugins":  state.Plugins,
			"base":     state.ScriptsPath,
		})
	})

	api.POST("/extensions/reload", func(c *gin.Context) {
		// 1. Prepare new state LOCALLY (no lock needed yet)
		newRuntime := goja.New()
		newCommands := make(map[string]*Command)
		newPlugins := make([]Plugin, 0)

		// 2. Setup API binding to local state
		setupAxAPI(newRuntime, &newCommands, &newPlugins)

		// 3. Load scripts into new runtime
		err := loadMainExtension(newRuntime)
		if err != nil {
			c.JSON(500, gin.H{"error": err.Error()})
			return
		}

		// 4. Atomic Swap
		state.mu.Lock()
		state.Runtime = newRuntime
		state.Commands = newCommands
		state.Plugins = newPlugins
		state.mu.Unlock()

		c.JSON(200, gin.H{"status": "extensions reloaded", "cmd_count": len(newCommands)})
	})

	api.GET("/extensions/list", func(c *gin.Context) {
		subPath := c.Query("path")
		targetPath := filepath.Join(state.ScriptsPath, subPath)

		// Security: Prevent traversal
		absTarget, _ := filepath.Abs(targetPath)
		absBase, _ := filepath.Abs(state.ScriptsPath)
		if !strings.HasPrefix(absTarget, absBase) {
			c.JSON(400, gin.H{"error": "Path traversal attempt"})
			return
		}

		entries, err := os.ReadDir(targetPath)
		if err != nil {
			c.JSON(500, gin.H{"error": err.Error()})
			return
		}

		var scripts []map[string]interface{}
		for _, entry := range entries {
			info, _ := entry.Info()
			scripts = append(scripts, map[string]interface{}{
				"name":   entry.Name(),
				"path":   filepath.Join(subPath, entry.Name()),
				"is_dir": entry.IsDir(),
				"size":   info.Size(),
			})
		}
		c.JSON(200, gin.H{"ok": true, "scripts": scripts, "base": state.ScriptsPath})
	})

	api.POST("/extensions/read", func(c *gin.Context) {
		var req struct {
			Path string `json:"path"`
		}
		if err := c.ShouldBindJSON(&req); err != nil {
			c.JSON(400, gin.H{"error": "Invalid request"})
			return
		}

		targetPath := filepath.Join(state.ScriptsPath, req.Path)
		absTarget, _ := filepath.Abs(targetPath)
		absBase, _ := filepath.Abs(state.ScriptsPath)
		if !strings.HasPrefix(absTarget, absBase) {
			c.JSON(400, gin.H{"error": "Access denied"})
			return
		}

		content, err := os.ReadFile(targetPath)
		if err != nil {
			c.JSON(500, gin.H{"error": err.Error()})
			return
		}
		c.JSON(200, gin.H{"ok": true, "content": string(content), "path": req.Path})
	})

	api.POST("/extensions/write", func(c *gin.Context) {
		var req struct {
			Path    string `json:"path"`
			Content string `json:"content"`
		}
		if err := c.ShouldBindJSON(&req); err != nil {
			c.JSON(400, gin.H{"error": "Invalid request"})
			return
		}

		targetPath := filepath.Join(state.ScriptsPath, req.Path)
		absTarget, _ := filepath.Abs(targetPath)
		absBase, _ := filepath.Abs(state.ScriptsPath)
		if !strings.HasPrefix(absTarget, absBase) {
			c.JSON(400, gin.H{"error": "Access denied"})
			return
		}

		err := os.WriteFile(targetPath, []byte(req.Content), 0644)
		if err != nil {
			c.JSON(500, gin.H{"error": err.Error()})
			return
		}
		c.JSON(200, gin.H{"ok": true, "path": req.Path})
	})

	api.POST("/agent/command/execute", func(c *gin.Context) {
		var req struct {
			ID          string                 `json:"id"`
			Name        string                 `json:"name"`
			Command     string                 `json:"command"`
			Cmdline     string                 `json:"cmdline"`
			Data        map[string]interface{} `json:"data"`
			UI          bool                   `json:"ui"`
			AxHookId    string                 `json:"ax_hook_id"`
			AxHandlerId string                 `json:"ax_handler_id"`
		}
		if err := c.ShouldBindJSON(&req); err != nil {
			c.JSON(400, gin.H{"error": "Invalid request"})
			return
		}

		// Log incoming request
		reqBytes, _ := json.Marshal(req)
		fmt.Printf("📥 [Gateway] Incoming /agent/command/execute: %s\n", string(reqBytes))

		// IMPORTANT: Do NOT hold state locks while executing JS hooks.
		// Hooks often call ax.execute_alias / handler registration which requires write locks.
		// If we keep a read lock here, we can deadlock (RLock held  JS tries Lock).
		state.mu.RLock()
		cmdTree := state.Commands
		runtime := state.Runtime
		state.mu.RUnlock()

		// 1. Resolve Command (Support SubCommands)
		parts := unserializeParams(strings.TrimSpace(req.Cmdline))
		cmd, remainingParts, _ := findCommand(cmdTree, parts)

		// 2. If command found and has a PreHook, execute it locally
		if cmd != nil && cmd.PreHook != nil && !goja.IsUndefined(cmd.PreHook) {
			fmt.Printf("🪝 [Gateway] Triggering pre-hook for command: %s (Agent: %s)\n", cmd.Name, req.Name)

			// Auto-parse args if data is empty (CLI input)
			parsedData := req.Data
			if len(parsedData) == 0 {
				parsedData = parseArgs(cmd, remainingParts)
			}

			fn, ok := goja.AssertFunction(cmd.PreHook)
			if !ok {
				fmt.Printf("❌ [Gateway] Pre-hook is not a function\n")
				status, body, _ := forwardCommandToC2(c, req.ID, req.Cmdline, req.Name, parsedData, req.UI, req.AxHookId, req.AxHandlerId)
				c.Data(status, "application/json", body)
				return
			}

			// Set CurrentLoadingDir to the command's script directory before execution
			// so that ax.script_dir() returns the correct path for relative resource loading.
			state.mu.Lock()
			oldDir := state.CurrentLoadingDir
			state.CurrentLoadingDir = cmd.ScriptDir
			state.mu.Unlock()

			// Call JS PreHook(id, cmdline, parsed_json, ...parsed_lines)
			_, err := fn(goja.Undefined(),
				runtime.ToValue(req.ID),
				runtime.ToValue(req.Cmdline),
				runtime.ToValue(parsedData),
			)

			state.mu.Lock()
			state.CurrentLoadingDir = oldDir
			state.mu.Unlock()
			if err != nil {
				fmt.Printf("❌ [Gateway] Pre-hook execution error: %v\n", err)
				c.JSON(200, gin.H{"ok": false, "message": fmt.Sprintf("Extension hook failed: %v", err)})
				return
			}
			c.JSON(200, gin.H{"ok": true, "message": "Handled by extension hook"})
			return
		}

		// 3. Fallback: Forward to C2
		fmt.Printf("⏩ [Gateway] Forwarding command to C2: %s (Agent: %s)\n", req.Cmdline, req.Name)
		status, body, _ := forwardCommandToC2(c, req.ID, req.Cmdline, req.Name, req.Data, req.UI, req.AxHookId, req.AxHandlerId)
		c.Data(status, "application/json", body)
	})

	// --- 5. MCP Endpoint ---
	mcp := r.Group("/mcp")
	mcp.GET("/tools", handleMCPListTools)
	mcp.POST("/call", handleMCPCallTool)

	// --- 1. Web UI Hosting ---
	// Define Static and NoRoute LAST
	r.Static("/ui", "./static")

	r.NoRoute(func(c *gin.Context) {
		path := c.Request.URL.Path
		// If it's a UI route (not /api/ or /mcp/), serve index.html to support React Router history mode
		if strings.HasPrefix(path, "/ui") {
			// Check if it's a static file request (has extension)
			ext := filepath.Ext(path)
			if ext != "" && ext != ".html" {
				c.JSON(http.StatusNotFound, gin.H{"error": "Static resource not found"})
				return
			}
			c.File("./static/index.html")
			return
		}
		c.JSON(http.StatusNotFound, gin.H{"error": "Resource not found"})
	})

	r.GET("/", func(c *gin.Context) {
		c.Redirect(http.StatusMovedPermanently, "/ui/")
	})

	fmt.Println("🚀 Adaptix Web-Gateway (V2) Active")
	fmt.Printf("🛡️ UI: http://localhost%s/ui\n", *listenAddr)
	fmt.Printf("🤖 MCP: http://localhost%s/mcp\n", *listenAddr)

	r.Run(*listenAddr)
}

// --- JS Engine Setup ---

func setupAxAPI(vm *goja.Runtime, commands *map[string]*Command, plugins *[]Plugin) {
	ax := vm.NewObject()
	menuObj := vm.NewObject()

	// ax.script_dir()
	ax.Set("script_dir", func(call goja.FunctionCall) goja.Value {
		state.mu.RLock()
		dir := state.CurrentLoadingDir
		state.mu.RUnlock()

		if dir == "" {
			absPath, _ := filepath.Abs(state.ScriptsPath)
			return vm.ToValue(absPath + "/")
		}
		return vm.ToValue(dir + "/")
	})

	// ax.script_load(path) / ax.script_import(path)
	loadFunc := func(call goja.FunctionCall) goja.Value {
		path := call.Argument(0).String()

		// If path is relative, try to resolve it against CurrentLoadingDir then ScriptsPath
		targetPath := path
		if !filepath.IsAbs(path) {
			state.mu.RLock()
			currentDir := state.CurrentLoadingDir
			state.mu.RUnlock()

			if currentDir != "" {
				tmp := filepath.Join(currentDir, path)
				if _, err := os.Stat(tmp); err == nil {
					targetPath = tmp
				} else {
					targetPath = filepath.Join(state.ScriptsPath, path)
				}
			} else {
				targetPath = filepath.Join(state.ScriptsPath, path)
			}
		}

		fmt.Printf("📜 [Gateway] Loading script: %s\n", targetPath)
		content, err := ioutil.ReadFile(targetPath)
		if err != nil {
			fmt.Printf("⚠️ [Gateway] Failed to read script %s: %v\n", targetPath, err)
			return vm.ToValue(false)
		}

		// Update CurrentLoadingDir for relative paths in scripts
		absPath, _ := filepath.Abs(targetPath)
		oldDir := state.CurrentLoadingDir
		state.mu.Lock()
		state.CurrentLoadingDir = filepath.Dir(absPath)
		state.mu.Unlock()

		_, err = vm.RunString(string(content))

		state.mu.Lock()
		state.CurrentLoadingDir = oldDir
		state.mu.Unlock()

		if err != nil {
			fmt.Printf("❌ [Gateway] Script error in %s: %v\n", targetPath, err)
			return vm.ToValue(false)
		}
		return vm.ToValue(true)
	}
	ax.Set("script_load", loadFunc)
	ax.Set("script_import", loadFunc)

	// ax.create_command(name, description, example)
	ax.Set("create_command", func(call goja.FunctionCall) goja.Value {
		name := call.Argument(0).String()
		desc := call.Argument(1).String()
		example := ""
		if len(call.Arguments) > 2 {
			example = call.Arguments[2].String()
		}

		cmdObj := vm.NewObject()
		cmdObj.Set("name", name)
		cmdObj.Set("description", desc)
		cmdObj.Set("example", example)

		// Internal tracking for this specific command instance
		state.mu.RLock()
		currentDir := state.CurrentLoadingDir
		state.mu.RUnlock()
		c := &Command{Name: name, Description: desc, Example: example, Args: make([]interface{}, 0), ScriptDir: currentDir}

		cmdObj.Set("addArgString", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "string",
				"required":    true,
			}
			if len(call.Arguments) > 2 {
				arg["required"] = !call.Argument(2).ToBoolean()
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgInt", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "int",
				"required":    true,
			}
			if len(call.Arguments) > 2 {
				arg["required"] = !call.Argument(2).ToBoolean()
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgBool", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "bool",
				"required":    false,
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgFile", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "file",
				"required":    true,
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgFlagString", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "flag_string",
				"mark":        call.Argument(2).String(),
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgFlagInt", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "flag_int",
				"mark":        call.Argument(2).String(),
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("addArgFlagFile", func(call goja.FunctionCall) goja.Value {
			arg := map[string]interface{}{
				"name":        call.Argument(0).String(),
				"description": call.Argument(1).String(),
				"type":        "flag_file",
				"mark":        call.Argument(2).String(),
			}
			c.Args = append(c.Args, arg)
			return cmdObj
		})
		cmdObj.Set("add_session_access", func(call goja.FunctionCall) goja.Value {
			// Stub for scripts that expect this (e.g., creds.axs, elevate.axs)
			return cmdObj
		})
		cmdObj.Set("add_processbrowser", func(call goja.FunctionCall) goja.Value {
			// Stub for scripts that expect this (e.g., inject.axs, lateral.axs)
			return cmdObj
		})
		cmdObj.Set("create_action", func(call goja.FunctionCall) goja.Value {
			// Stub for scripts that expect this
			return cmdObj
		})
		cmdObj.Set("addSubCommands", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				arrObj := call.Argument(0).ToObject(vm)
				if arrObj != nil {
					if c.SubCommands == nil {
						c.SubCommands = make(map[string]*Command)
					}
					length := int(arrObj.Get("length").ToInteger())
					for i := 0; i < length; i++ {
						v := arrObj.Get(strconv.Itoa(i))
						if goja.IsUndefined(v) || goja.IsNull(v) {
							continue
						}
						o := v.ToObject(vm)
						if o == nil {
							continue
						}
						internal := o.Get("__internal")
						if !goja.IsUndefined(internal) && !goja.IsNull(internal) {
							if ic, ok := internal.Export().(*Command); ok && ic != nil {
								c.SubCommands[strings.ToLower(ic.Name)] = ic
								continue
							}
						}
						// Fallback: name/desc/example only
						name := o.Get("name").String()
						c.SubCommands[strings.ToLower(name)] = &Command{
							Name:        name,
							Description: o.Get("description").String(),
							Example:     o.Get("example").String(),
						}
					}
				}
			}
			return cmdObj
		})

		cmdObj.Set("setPreHook", func(call goja.FunctionCall) goja.Value {
			c.PreHook = call.Argument(0)
			if gatewayDebugMode {
				fmt.Printf("🔗 [Gateway] Hook registered for command: %s\n", name)
			}
			return cmdObj
		})

		cmdObj.Set("__internal", c)
		return cmdObj
	})

	// ax.register_commands_group(group, agents, os, listeners)
	ax.Set("register_commands_group", func(call goja.FunctionCall) goja.Value {
		groupObj := call.Argument(0).ToObject(vm)
		groupName := groupObj.Get("groupName").String()

		var filterAgents []string
		if len(call.Arguments) > 1 {
			if arr, ok := call.Argument(1).Export().([]interface{}); ok {
				for _, v := range arr {
					filterAgents = append(filterAgents, fmt.Sprint(v))
				}
			}
		}
		var filterOS []string
		if len(call.Arguments) > 2 {
			if arr, ok := call.Argument(2).Export().([]interface{}); ok {
				for _, v := range arr {
					filterOS = append(filterOS, fmt.Sprint(v))
				}
			}
		}
		var filterListeners []string
		if len(call.Arguments) > 3 {
			if arr, ok := call.Argument(3).Export().([]interface{}); ok {
				for _, v := range arr {
					filterListeners = append(filterListeners, fmt.Sprint(v))
				}
			}
		}

		if cmdsVal := groupObj.Get("commands"); cmdsVal != nil && !goja.IsUndefined(cmdsVal) {
			obj := cmdsVal.ToObject(vm)
			length := obj.Get("length").ToInteger()

			for i := int64(0); i < length; i++ {
				cmdVal := obj.Get(fmt.Sprintf("%d", i))
				cmdObj := cmdVal.ToObject(vm)

				internal := cmdObj.Get("__internal")
				var c *Command
				if internal != nil && !goja.IsUndefined(internal) {
					c = internal.Export().(*Command)
					c.Group = groupName
					(*commands)[strings.ToLower(c.Name)] = c
				} else {
					name := cmdObj.Get("name").String()
					c = &Command{
						Name:        name,
						Description: cmdObj.Get("description").String(),
						Example:     cmdObj.Get("example").String(),
						Group:       groupName,
						Args:        make([]interface{}, 0),
					}
					(*commands)[strings.ToLower(name)] = c
				}

				*plugins = append(*plugins, Plugin{
					Category:  groupName,
					Command:   c.Name,
					Agents:    filterAgents,
					OS:        filterOS,
					Listeners: filterListeners,
				})
				fmt.Printf("📦 [Gateway] Registered command from group %s: %s\n", groupName, c.Name)
			}
		}
		return goja.Undefined()
	})

	// ax.register_plugin(category, command, agents, os, callback)
	ax.Set("register_plugin", func(call goja.FunctionCall) goja.Value {
		category := call.Argument(0).String()
		command := call.Argument(1).String()

		var filterAgents []string
		if arr, ok := call.Argument(2).Export().([]interface{}); ok {
			for _, v := range arr {
				filterAgents = append(filterAgents, fmt.Sprint(v))
			}
		}
		var filterOS []string
		if arr, ok := call.Argument(3).Export().([]interface{}); ok {
			for _, v := range arr {
				filterOS = append(filterOS, fmt.Sprint(v))
			}
		}

		p := Plugin{
			Category: category,
			Command:  command,
			Agents:   filterAgents,
			OS:       filterOS,
		}
		if len(call.Arguments) > 4 {
			p.Callback = call.Argument(4)
		}

		*plugins = append(*plugins, p)
		fmt.Printf("🔌 [Gateway] Registered manual plugin: %s/%s\n", category, command)
		return goja.Undefined()
	})

	ax.Set("create_commands_group", func(call goja.FunctionCall) goja.Value {
		name := call.Argument(0).String()
		cmds := call.Argument(1)
		group := vm.NewObject()
		group.Set("groupName", name)
		group.Set("commands", cmds)
		return group
	})

	// ax.bof_pack(types, args)
	ax.Set("bof_pack", func(call goja.FunctionCall) goja.Value {
		typesStr := call.Argument(0).String()
		argsVal := call.Argument(1).Export()
		args, ok := argsVal.([]interface{})
		if !ok {
			return vm.ToValue("")
		}

		typeList := strings.Split(typesStr, ",")
		buf := new(bytes.Buffer)

		for i, t := range typeList {
			t = strings.TrimSpace(t)
			if i >= len(args) {
				break
			}
			val := args[i]

			switch t {
			case "cstr":
				s := fmt.Sprint(val)
				b := []byte(s)
				binary.Write(buf, binary.LittleEndian, uint32(len(b)+1))
				buf.Write(b)
				buf.WriteByte(0)
			case "wstr":
				s := fmt.Sprint(val)
				u16 := utf16.Encode([]rune(s))
				binary.Write(buf, binary.LittleEndian, uint32((len(u16)+1)*2))
				for _, v := range u16 {
					binary.Write(buf, binary.LittleEndian, v)
				}
				binary.Write(buf, binary.LittleEndian, uint16(0))
			case "int":
				var n int32
				switch v := val.(type) {
				case int64:
					n = int32(v)
				case int:
					n = int32(v)
				case float64:
					n = int32(v)
				default:
					n = 0
				}
				binary.Write(buf, binary.LittleEndian, n)
			case "short":
				var n int16
				switch v := val.(type) {
				case int64:
					n = int16(v)
				case int:
					n = int16(v)
				case float64:
					n = int16(v)
				default:
					n = 0
				}
				binary.Write(buf, binary.LittleEndian, n)
			case "bytes":
				s := fmt.Sprint(val)
				b, err := base64.StdEncoding.DecodeString(s)
				if err != nil {
					b = []byte(s)
				}
				binary.Write(buf, binary.LittleEndian, uint32(len(b)))
				buf.Write(b)
			}
		}

		finalBuf := new(bytes.Buffer)
		binary.Write(finalBuf, binary.LittleEndian, uint32(buf.Len()))
		finalBuf.Write(buf.Bytes())
		return vm.ToValue(base64.StdEncoding.EncodeToString(finalBuf.Bytes()))
	})

	ax.Set("arch", func(call goja.FunctionCall) goja.Value {
		id := call.Argument(0).String()
		state.mu.RLock()
		defer state.mu.RUnlock()
		if agent, exists := state.ActiveAgents[id]; exists {
			if aMap, ok := agent.(map[string]interface{}); ok {
				return vm.ToValue(aMap["a_arch"])
			}
		}
		return vm.ToValue("x64")
	})

	// ax.agents() -> map[id]agentInfo
	// Used by post-hooks to enrich parsed task output (e.g. creds.axs hashdump hook).
	ax.Set("agents", func(call goja.FunctionCall) goja.Value {
		state.mu.RLock()
		defer state.mu.RUnlock()
		out := vm.NewObject()
		for id, raw := range state.ActiveAgents {
			a, ok := raw.(map[string]interface{})
			if !ok {
				continue
			}
			agentObj := vm.NewObject()
			// Provide Qt-like short keys expected by scripts.
			agentObj.Set("id", getPktString(a, "a_id"))
			agentObj.Set("type", getPktString(a, "a_name"))
			agentObj.Set("computer", getPktString(a, "a_computer"))
			agentObj.Set("domain", getPktString(a, "a_domain"))
			agentObj.Set("username", getPktString(a, "a_username"))
			agentObj.Set("internal_ip", getPktString(a, "a_internal_ip"))
			agentObj.Set("external_ip", getPktString(a, "a_external_ip"))
			agentObj.Set("arch", getPktString(a, "a_arch"))
			_ = out.Set(id, agentObj)
		}
		return out
	})

	ax.Set("agent_info", func(call goja.FunctionCall) goja.Value {
		id := call.Argument(0).String()
		prop := call.Argument(1).String()
		state.mu.RLock()
		defer state.mu.RUnlock()
		if agent, exists := state.ActiveAgents[id]; exists {
			if aMap, ok := agent.(map[string]interface{}); ok {
				if prop == "os" {
					osInt := getPktInt(aMap, "a_os")
					switch osInt {
					case 1:
						return vm.ToValue("windows")
					case 2:
						return vm.ToValue("linux")
					case 3:
						return vm.ToValue("macos")
					default:
						return vm.ToValue("unknown")
					}
				}
				propMap := map[string]string{
					"id": "a_id", "type": "a_name", "listener": "a_listener",
					"external_ip": "a_external_ip", "internal_ip": "a_internal_ip",
					"domain": "a_domain", "computer": "a_computer", "username": "a_username",
					"impersonated": "a_impersonated", "process": "a_process", "arch": "a_arch",
					"pid": "a_pid", "tid": "a_tid", "gmt": "a_gmt_offset", "acp": "a_acp", "oemcp": "a_oemcp",
					"elevated": "a_elevated", "tags": "a_tags", "async": "a_async", "sleep": "a_sleep", "os_full": "a_os_desc",
				}
				field := propMap[prop]
				if field == "" {
					field = prop
				}
				if val, exists := aMap[field]; exists {
					return vm.ToValue(val)
				}
			}
		}
		return goja.Undefined()
	})

	ax.Set("execute_browser", func(call goja.FunctionCall) goja.Value {
		id := call.Argument(0).String()
		cmdline := call.Argument(1).String()
		go func() {
			forwardCommandToC2(nil, id, cmdline, "browser", map[string]interface{}{}, true, "", "")
		}()
		return goja.Undefined()
	})

	// ax.execute_alias(id, cmdline, alias, message, callback)
	doExecuteAlias := func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 3 {
			return goja.Undefined()
		}
		id := call.Argument(0).String()
		cmdline := call.Argument(1).String()
		alias := call.Argument(2).String()
		message := ""
		if len(call.Arguments) > 3 {
			message = call.Argument(3).String()
		}
		if cmdline != "" {
			state.mu.Lock()
			state.AliasDisplayCmd[id] = cmdline
			state.AliasDisplayTime[id] = time.Now().Unix()
			state.mu.Unlock()
		}

		handlerId := ""
		if len(call.Arguments) > 4 {
			handlerFn := call.Argument(4)
			if !goja.IsUndefined(handlerFn) && !goja.IsNull(handlerFn) {
				handlerId = fmt.Sprintf("h_%d", time.Now().UnixNano())
				state.mu.Lock()
				state.Handlers[handlerId] = handlerFn
				state.mu.Unlock()
			}
		}

		go func() {
			status, body, err := forwardCommandToC2(nil, id, alias, "alias", map[string]interface{}{"message": message}, true, "", handlerId)
			if err != nil {
				fmt.Printf("❌ [Gateway] Alias command error: %v\n", err)
			} else {
				fmt.Printf("📥 [Gateway] C2 Response for Alias (Status %d): %s\n", status, string(body))
			}
		}()
		return goja.Undefined()
	}
	ax.Set("execute_alias", doExecuteAlias)
	ax.Set("execute_alias_handler", doExecuteAlias)

	ax.Set("credentials", func(call goja.FunctionCall) goja.Value {
		state.mu.RLock()
		defer state.mu.RUnlock()
		return vm.ToValue(state.Credentials)
	})

	ax.Set("targets", func(call goja.FunctionCall) goja.Value {
		state.mu.RLock()
		defer state.mu.RUnlock()
		return vm.ToValue(state.Targets)
	})

	ax.Set("credentials_add_list", func(call goja.FunctionCall) goja.Value {
		list := call.Argument(0).Export()
		go func() {
			_, body, err := httpPostToC2("/creds/add", map[string]interface{}{"creds": list})
			if err != nil {
				fmt.Printf("❌ [Script] credentials_add_list failed: %v\n", err)
			} else {
				fmt.Printf("✅ [Script] Added credentials: %s\n", string(body))
			}
		}()
		return goja.Undefined()
	})

	ax.Set("targets_add_list", func(call goja.FunctionCall) goja.Value {
		list := call.Argument(0).Export()
		go func() {
			_, body, err := httpPostToC2("/targets/add", map[string]interface{}{"targets": list})
			if err != nil {
				fmt.Printf("❌ [Script] targets_add_list failed: %v\n", err)
			} else {
				fmt.Printf("✅ [Script] Added targets: %s\n", string(body))
			}
		}()
		return goja.Undefined()
	})

	ax.Set("agent_remove", func(call goja.FunctionCall) goja.Value {
		ids := call.Argument(0).Export()
		go func() {
			httpPostToC2("/agent/remove", map[string]interface{}{"agent_id_array": ids})
		}()
		return goja.Undefined()
	})

	ax.Set("agent_set_color", func(call goja.FunctionCall) goja.Value {
		ids := call.Argument(0).Export()
		bg := call.Argument(1).String()
		fg := call.Argument(2).String()
		reset := call.Argument(3).ToBoolean()
		go func() {
			httpPostToC2("/agent/set/color", map[string]interface{}{"agent_id_array": ids, "bc": bg, "fc": fg, "reset": reset})
		}()
		return goja.Undefined()
	})

	ax.Set("agent_set_mark", func(call goja.FunctionCall) goja.Value {
		ids := call.Argument(0).Export()
		mark := call.Argument(1).String()
		go func() {
			httpPostToC2("/agent/set/mark", map[string]interface{}{"agent_id_array": ids, "mark": mark})
		}()
		return goja.Undefined()
	})

	ax.Set("agent_set_impersonate", func(call goja.FunctionCall) goja.Value {
		id := call.Argument(0).String()
		user := call.Argument(1).String()
		elevated := call.Argument(2).ToBoolean()
		val := user
		if elevated {
			val += " *"
		}
		go func() {
			httpPostToC2("/agent/update/data", map[string]interface{}{"agent_id": id, "impersonated": val})
		}()
		return goja.Undefined()
	})

	ax.Set("log", func(call goja.FunctionCall) goja.Value {
		msg := call.Argument(0).String()
		fmt.Printf("📜 [Script] %s\n", msg)
		broadcastToWebUI(map[string]interface{}{
			"type":       0x13, // SP_TYPE_EVENT
			"event_type": 5,    // EVENT_AGENT_NEW style info
			"date":       time.Now().Unix(),
			"message":    "[Script] " + msg,
		})
		return goja.Undefined()
	})

	ax.Set("log_error", func(call goja.FunctionCall) goja.Value {
		msg := call.Argument(0).String()
		fmt.Printf("❌ [Script Error] %s\n", msg)
		broadcastToWebUI(map[string]interface{}{
			"type":       0x13, // SP_TYPE_EVENT
			"event_type": 2,    // EVENT_CLIENT_DISCONNECT style error
			"date":       time.Now().Unix(),
			"message":    "[Script Error] " + msg,
		})
		return goja.Undefined()
	})

	ax.Set("console_message", func(call goja.FunctionCall) goja.Value {
		id := call.Argument(0).String()
		message := call.Argument(1).String()
		fmt.Printf("🖥️ [Script Console] Agent: %s, Message: %s\n", id, message)

		// Broadcast as TYPE_AGENT_CONSOLE_OUT so it appears in the Web UI console
		broadcastToWebUI(map[string]interface{}{
			"type":       0x69, // TYPE_AGENT_CONSOLE_OUT
			"a_id":       id,
			"a_msg_type": 5, // CONSOLE_OUT_INFO
			"a_text":     message,
			"time":       time.Now().Unix(),
		})
		return goja.Undefined()
	})

	vm.Set("ax", ax)

	// menu.* APIs (Qt BridgeMenu compatibility). These are primarily used by Extension-Kit scripts
	// to register UI context menu actions. Web currently doesn't render these menus, but we must
	// provide the API surface so scripts don't fail to load.
	menuObj.Set("create_action", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		if len(call.Arguments) > 0 {
			obj.Set("text", call.Argument(0))
		}
		if len(call.Arguments) > 1 {
			obj.Set("handler", call.Argument(1))
		}
		return obj
	})
	menuObj.Set("create_menu", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		if len(call.Arguments) > 0 {
			obj.Set("title", call.Argument(0))
		}
		obj.Set("addItem", func(goja.FunctionCall) goja.Value {
			return goja.Undefined()
		})
		return obj
	})
	menuObj.Set("create_separator", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		obj.Set("type", "separator")
		return obj
	})

	// Registration functions (no-op for now)
	for _, fnName := range []string{
		"add_session_main",
		"add_session_agent",
		"add_session_browser",
		"add_session_access",
		"add_filebrowser",
		"add_processbrowser",
		"add_downloads_running",
		"add_downloads_finished",
		"add_tasks",
		"add_tasks_job",
		"add_targets",
		"add_credentials",
	} {
		name := fnName
		menuObj.Set(name, func(call goja.FunctionCall) goja.Value {
			// Intentionally no-op; keep scripts compatible.
			return goja.Undefined()
		})
	}

	vm.Set("menu", menuObj)
	vm.Set("form", vm.NewObject()) // Stub
}

func loadMainExtension(vm *goja.Runtime) error {
	mainScript := filepath.Join(state.ScriptsPath, "extension-kit.axs")
	absMain, _ := filepath.Abs(mainScript)

	content, err := ioutil.ReadFile(absMain)
	if err != nil {
		return err
	}

	state.mu.Lock()
	oldDir := state.CurrentLoadingDir
	state.CurrentLoadingDir = filepath.Dir(absMain)
	state.mu.Unlock()

	_, _ = vm.RunString("var metadata = { name: 'root' };")
	_, err = vm.RunString(string(content))

	state.mu.Lock()
	state.CurrentLoadingDir = oldDir
	state.mu.Unlock()

	if err != nil {
		fmt.Printf("❌ [Gateway] Extension-Kit error: %v\n", err)
	}
	return err
}

func handleMCPListTools(c *gin.Context) {
	tools := []map[string]interface{}{
		{
			"name":        "list_agents",
			"description": "List all connected agents",
		},
		{
			"name":        "execute_command",
			"description": "Execute a command on an agent",
			"inputSchema": map[string]interface{}{
				"type": "object",
				"properties": map[string]interface{}{
					"agent_id": map[string]interface{}{"type": "string"},
					"command":  map[string]interface{}{"type": "string"},
				},
				"required": []string{"agent_id", "command"},
			},
		},
	}
	c.JSON(200, gin.H{"tools": tools})
}

func handleMCPCallTool(c *gin.Context) {
	var call struct {
		Name      string                 `json:"name"`
		Arguments map[string]interface{} `json:"arguments"`
	}
	if err := c.ShouldBindJSON(&call); err != nil {
		return
	}

	state.mu.RLock()
	defer state.mu.RUnlock()

	switch call.Name {
	case "list_agents":
		var list []string
		for id, agent := range state.ActiveAgents {
			a := agent.(map[string]interface{})
			name := fmt.Sprint(a["a_name"])
			os := fmt.Sprint(a["a_os_desc"])
			arch := fmt.Sprint(a["a_arch"])
			list = append(list, fmt.Sprintf("- %s: %s (%s, %s)", id, name, os, arch))
		}
		if len(list) == 0 {
			c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": "No active agents found."}}})
		} else {
			c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": "Active Agents:\n" + strings.Join(list, "\n")}}})
		}

	case "get_agent_info":
		id := fmt.Sprint(call.Arguments["agent_id"])
		if agent, exists := state.ActiveAgents[id]; exists {
			info, _ := json.MarshalIndent(agent, "", "  ")
			c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": string(info)}}})
		} else {
			c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": "Agent not found: " + id}}})
		}

	case "execute_command":
		id := fmt.Sprint(call.Arguments["agent_id"])
		cmdline := fmt.Sprint(call.Arguments["command"])
		// Internal trigger via MCP
		go forwardCommandToC2(nil, id, cmdline, "", map[string]interface{}{}, true, "", "")
		c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": "Command issued to agent " + id}}})

	default:
		c.JSON(200, gin.H{"content": []map[string]interface{}{{"type": "text", "text": "Tool " + call.Name + " executed via gateway."}}})
	}
}
