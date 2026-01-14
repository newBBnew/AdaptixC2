package msf

import (
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

type WSHandler struct {
	api        *API
	upgrader   websocket.Upgrader
	clients    map[string]*WSClient
	clientMu   sync.RWMutex
	pollTicker *time.Ticker
	stopChan   chan struct{}
}

type WSClient struct {
	UserID     string
	Conn       *websocket.Conn
	ConsoleIDs map[string]bool
	SessionIDs map[string]bool
	Send       chan []byte
}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func mustMarshal(v interface{}) []byte {
	data, _ := json.Marshal(v)
	return data
}

func NewWSHandler(api *API) *WSHandler {
	return &WSHandler{
		api:      api,
		upgrader: upgrader,
		clients:  make(map[string]*WSClient),
		stopChan: make(chan struct{}),
	}
}

func (h *WSHandler) HandleConnect(userID string, conn *websocket.Conn) error {
	client := &WSClient{
		UserID:     userID,
		Conn:       conn,
		ConsoleIDs: make(map[string]bool),
		SessionIDs: make(map[string]bool),
		Send:       make(chan []byte, 256),
	}

	h.clientMu.Lock()
	h.clients[userID] = client
	h.clientMu.Unlock()

	go h.writePump(client)
	go h.readPump(client)
	go h.pollPump(client)

	return nil
}

func (h *WSHandler) HandleDisconnect(userID string) {
	h.clientMu.Lock()
	defer h.clientMu.Unlock()

	if client, ok := h.clients[userID]; ok {
		close(client.Send)
		delete(h.clients, userID)
	}
}

func (h *WSHandler) Broadcast(msgType string, data interface{}) {
	h.clientMu.RLock()
	defer h.clientMu.RUnlock()

	msg := WSMessage{Type: msgType, Data: data}
	for _, client := range h.clients {
		select {
		case client.Send <- mustMarshal(msg):
		default:
		}
	}
}

func (h *WSHandler) SendToUser(userID string, msgType string, data interface{}) {
	h.clientMu.RLock()
	client, ok := h.clients[userID]
	h.clientMu.RUnlock()

	if ok {
		select {
		case client.Send <- mustMarshal(WSMessage{Type: msgType, Data: data}):
		default:
		}
	}
}

func (h *WSHandler) AddUserConsole(userID, consoleID string) {
	h.clientMu.Lock()
	if client, ok := h.clients[userID]; ok {
		client.ConsoleIDs[consoleID] = true
	}
	h.clientMu.Unlock()
}

func (h *WSHandler) RemoveUserConsole(userID, consoleID string) {
	h.clientMu.Lock()
	if client, ok := h.clients[userID]; ok {
		delete(client.ConsoleIDs, consoleID)
	}
	h.clientMu.Unlock()
}

func (h *WSHandler) AddUserSession(userID, sessionID string) {
	h.clientMu.Lock()
	if client, ok := h.clients[userID]; ok {
		client.SessionIDs[sessionID] = true
	}
	h.clientMu.Unlock()
}

func (h *WSHandler) RemoveUserSession(userID, sessionID string) {
	h.clientMu.Lock()
	if client, ok := h.clients[userID]; ok {
		delete(client.SessionIDs, sessionID)
	}
	h.clientMu.Unlock()
}

func (h *WSHandler) writePump(client *WSClient) {
	defer func() {
		client.Conn.Close()
	}()

	for {
		select {
		case message, ok := <-client.Send:
			client.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if !ok {
				client.Conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			if err := client.Conn.WriteMessage(websocket.TextMessage, message); err != nil {
				return
			}
		}
	}
}

func (h *WSHandler) readPump(client *WSClient) {
	defer func() {
		h.HandleDisconnect(client.UserID)
		client.Conn.Close()
	}()

	client.Conn.SetReadLimit(512 * 1024)
	client.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
	client.Conn.SetPongHandler(func(string) error {
		client.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		return nil
	})

	for {
		_, message, err := client.Conn.ReadMessage()
		if err != nil {
			break
		}

		h.handleMessage(client, message)
	}
}

func (h *WSHandler) handleMessage(client *WSClient, message []byte) {
	var msg WSMessage
	if err := json.Unmarshal(message, &msg); err != nil {
		return
	}

	switch msg.Type {
	case "console_write":
		if data, ok := msg.Data.(map[string]interface{}); ok {
			consoleID, _ := data["console_id"].(string)
			command, _ := data["command"].(string)
			if consoleID != "" && command != "" {
				h.api.consoleManager.Write(consoleID, command)
			}
		}

	case "session_interact":
		if data, ok := msg.Data.(map[string]interface{}); ok {
			sessionID, _ := data["session_id"].(string)
			command, _ := data["command"].(string)
			_ = sessionID
			_ = command
		}
	}
}

func (h *WSHandler) pollPump(client *WSClient) {
	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			for consoleID := range client.ConsoleIDs {
				data, busy, _ := h.api.consoleManager.Read(consoleID)
				if data != "" {
					client.Send <- mustMarshal(WSMessage{
						Type: "console_output",
						Data: map[string]interface{}{
							"console_id": consoleID,
							"data":       data,
							"busy":       busy,
						},
					})
				}
			}

		case <-h.stopChan:
			return
		}
	}
}

func (h *WSHandler) StartBackgroundMonitor() {
	h.pollTicker = time.NewTicker(1 * time.Second)
	go func() {
		for range h.pollTicker.C {
			h.monitorSessions()
			h.monitorJobs()
		}
	}()
}

func (h *WSHandler) StopBackgroundMonitor() {
	if h.pollTicker != nil {
		h.pollTicker.Stop()
	}
	close(h.stopChan)
}

func (h *WSHandler) monitorSessions() {
	if !h.api.IsConnected() {
		return
	}

	sessions, err := h.api.client.SessionList()
	if err != nil {
		return
	}

	h.Broadcast("sessions_update", sessions)
}

func (h *WSHandler) monitorJobs() {
	if !h.api.IsConnected() {
		return
	}

	jobs, err := h.api.client.JobsList()
	if err != nil {
		return
	}

	h.Broadcast("jobs_update", jobs)
}

func (h *WSHandler) NotifySessionNew(session SessionInfo) {
	h.Broadcast("session_new", session)
}

func (h *WSHandler) NotifySessionClosed(sessionID string) {
	h.Broadcast("session_closed", map[string]string{"session_id": sessionID})
}

func (h *WSHandler) NotifyJobNew(job JobInfo) {
	h.Broadcast("job_new", job)
}

func (h *WSHandler) NotifyJobRemoved(jobID string) {
	h.Broadcast("job_removed", map[string]string{"job_id": jobID})
}
