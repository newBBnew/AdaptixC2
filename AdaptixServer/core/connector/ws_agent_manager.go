package connector

import (
	"crypto/rand"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"net/http"
	"sync"
	"time"

	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type wsAgentCallbacks struct {
	onAck           func(agentId string, channelID uint32)
	onData          func(agentId string, channelID uint32, data []byte)
	onClose         func(agentId string, channelID uint32)
	onSessionOpened func(agentId string)
	onSessionClosed func(agentId string)
}

type wsAgentManager struct {
	mu       sync.RWMutex
	sessions map[string]*wsAgentSession
	pending  map[string]*pendingSession
	cb       wsAgentCallbacks
}

type pendingSession struct {
	agentId string
	token   string
	expire  time.Time
}

type wsAgentSession struct {
	manager    *wsAgentManager
	agentId    string
	conn       *websocket.Conn
	send       chan []byte
	done       chan struct{}
	closeOnce  sync.Once
	channels   map[uint32]*wsAgentChannel
	channelsMu sync.RWMutex
}

type wsAgentChannel struct {
	acked bool
}

const wsAgentMaxMessageBytes = 8 * 1024 * 1024

func newWsAgentManager(cb wsAgentCallbacks) *wsAgentManager {
	return &wsAgentManager{
		sessions: make(map[string]*wsAgentSession),
		pending:  make(map[string]*pendingSession),
		cb:       cb,
	}
}

func wsFallbackToken() string {
	b := make([]byte, 16)
	if _, err := rand.Read(b); err == nil {
		return "fallback-" + hex.EncodeToString(b)
	}
	return fmt.Sprintf("fallback-%d", time.Now().UnixNano())
}

func (m *wsAgentManager) registerPending(agentId string, ttl time.Duration) string {
	token, err := krypt.GenerateUID(16)
	if err != nil {
		logs.Error("", "[WS-Agent] GenerateUID failed: %v", err)
		token = wsFallbackToken()
	}

	m.mu.Lock()
	m.pending[token] = &pendingSession{
		agentId: agentId,
		token:   token,
		expire:  time.Now().Add(ttl),
	}
	m.mu.Unlock()

	return token
}

func (m *wsAgentManager) cleanupExpiredPending() {
	now := time.Now()
	m.mu.Lock()
	for token, p := range m.pending {
		if p.expire.Before(now) {
			delete(m.pending, token)
		}
	}
	m.mu.Unlock()
}

func (m *wsAgentManager) validatePending(agentId string, token string) bool {
	m.cleanupExpiredPending()

	m.mu.RLock()
	defer m.mu.RUnlock()
	pending, ok := m.pending[token]
	if !ok {
		return false
	}
	return pending.agentId == agentId
}

func (m *wsAgentManager) accept(agentId string, token string, conn *websocket.Conn) error {
	m.cleanupExpiredPending()

	m.mu.Lock()
	pending, ok := m.pending[token]
	if !ok {
		m.mu.Unlock()
		return errors.New("invalid websocket token")
	}
	if pending.agentId != agentId {
		m.mu.Unlock()
		return errors.New("token-agent mismatch")
	}

	delete(m.pending, token)

	if existing := m.sessions[agentId]; existing != nil {
		existing.close()
	}

	session := &wsAgentSession{
		manager:  m,
		agentId:  agentId,
		conn:     conn,
		send:     make(chan []byte, 128),
		done:     make(chan struct{}),
		channels: make(map[uint32]*wsAgentChannel),
	}
	m.sessions[agentId] = session
	m.mu.Unlock()

	logs.Info("", "[WS-Agent] Agent %s connected via WebSocket", agentId)

	if m.cb.onSessionOpened != nil {
		m.cb.onSessionOpened(agentId)
	}

	session.start()
	return nil
}

func (m *wsAgentManager) get(agentId string) (*wsAgentSession, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	session, ok := m.sessions[agentId]
	return session, ok
}

func (m *wsAgentManager) remove(agentId string, session *wsAgentSession) {
	m.mu.Lock()
	current, ok := m.sessions[agentId]
	if ok && current == session {
		delete(m.sessions, agentId)
	}
	m.mu.Unlock()

	if ok && current == session && m.cb.onSessionClosed != nil {
		m.cb.onSessionClosed(agentId)
	}
}

func (m *wsAgentManager) sendTarget(agentId string, channelID uint32, target string) error {
	session, ok := m.get(agentId)
	if !ok {
		return errors.New("agent websocket not connected")
	}

	session.channelsMu.Lock()
	session.channels[channelID] = &wsAgentChannel{acked: false}
	session.channelsMu.Unlock()

	payload := []byte(target)
	msg := make([]byte, 4+len(payload))
	binary.LittleEndian.PutUint32(msg, channelID)
	copy(msg[4:], payload)

	return session.enqueue(msg)
}

func (m *wsAgentManager) sendData(agentId string, channelID uint32, data []byte) error {
	session, ok := m.get(agentId)
	if !ok {
		return errors.New("agent websocket not connected")
	}

	msg := make([]byte, 4+len(data))
	binary.LittleEndian.PutUint32(msg, channelID)
	copy(msg[4:], data)

	return session.enqueue(msg)
}

func (m *wsAgentManager) closeChannel(agentId string, channelID uint32) error {
	session, ok := m.get(agentId)
	if !ok {
		return errors.New("agent websocket not connected")
	}

	session.channelsMu.Lock()
	delete(session.channels, channelID)
	session.channelsMu.Unlock()

	msg := make([]byte, 4)
	binary.LittleEndian.PutUint32(msg, channelID)

	return session.enqueue(msg)
}

func (m *wsAgentManager) hasSession(agentId string) bool {
	_, ok := m.get(agentId)
	return ok
}

func (tc *TsConnector) WsTunnelRegisterPending(agentId string, ttl time.Duration) string {
	return tc.wsAgentMgr.registerPending(agentId, ttl)
}

func (tc *TsConnector) WsTunnelSendTarget(agentId string, channelID uint32, target string) error {
	return tc.wsAgentMgr.sendTarget(agentId, channelID, target)
}

func (tc *TsConnector) WsTunnelSendData(agentId string, channelID uint32, data []byte) error {
	return tc.wsAgentMgr.sendData(agentId, channelID, data)
}

func (tc *TsConnector) WsTunnelCloseChannel(agentId string, channelID uint32) error {
	return tc.wsAgentMgr.closeChannel(agentId, channelID)
}

func (tc *TsConnector) WsTunnelHasSession(agentId string) bool {
	return tc.wsAgentMgr.hasSession(agentId)
}

func (tc *TsConnector) tcWsAgentTunnel(ctx *gin.Context) {
	agentId := ctx.Query("agent_id")
	token := ctx.Query("token")

	if agentId == "" || token == "" {
		ctx.String(400, "missing agent_id or token")
		return
	}

	if !tc.teamserver.TsAgentIsExists(agentId) {
		ctx.String(404, "agent not found")
		return
	}

	if !tc.wsAgentMgr.validatePending(agentId, token) {
		ctx.String(401, "websocket token invalid")
		return
	}

	upgrader := websocket.Upgrader{
		HandshakeTimeout: 15 * time.Second,
		CheckOrigin:      func(r *http.Request) bool { return true },
	}

	wsConn, err := upgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		logs.Error("", "[WS-Agent] Upgrade failed for %s: %v", agentId, err)
		return
	}

	if err := tc.wsAgentMgr.accept(agentId, token, wsConn); err != nil {
		logs.Error("", "[WS-Agent] Accept failed for %s: %v", agentId, err)
		_ = wsConn.Close()
		return
	}
}

func (s *wsAgentSession) start() {
	s.conn.SetReadLimit(wsAgentMaxMessageBytes)
	go s.writer()
	go s.reader()
}

func (s *wsAgentSession) enqueue(msg []byte) error {
	select {
	case s.send <- msg:
		return nil
	case <-s.done:
		return errors.New("websocket session closed")
	}
}

func (s *wsAgentSession) writer() {
	for {
		select {
		case msg := <-s.send:
			if err := s.conn.WriteMessage(websocket.BinaryMessage, msg); err != nil {
				logs.Error("", "[WS-Agent] Write error for %s: %v", s.agentId, err)
				s.close()
				return
			}
		case <-s.done:
			return
		}
	}
}

func (s *wsAgentSession) reader() {
	for {
		_, message, err := s.conn.ReadMessage()
		if err != nil {
			logs.Debug("", "[WS-Agent] ReadMessage error for %s: %v", s.agentId, err)
			s.close()
			return
		}

		if len(message) < 4 {
			logs.Debug("", "[WS-Agent] Message too short from %s: %d bytes", s.agentId, len(message))
			continue
		}

		channelID := binary.LittleEndian.Uint32(message[:4])
		payload := message[4:]
		logs.Debug("", "[WS-Agent] Received message from %s: channelID=%d, payloadLen=%d", s.agentId, channelID, len(payload))

		var callback func(agentId string, channelID uint32)

		s.channelsMu.Lock()
		channelState, exists := s.channels[channelID]
		if !exists {
			channelState = &wsAgentChannel{}
			s.channels[channelID] = channelState
			logs.Debug("", "[WS-Agent] Created new channel state for %s channelID=%d", s.agentId, channelID)
		}

		if len(payload) == 0 {
			if !channelState.acked {
				channelState.acked = true
				callback = s.manager.cb.onAck
				logs.Debug("", "[WS-Agent] First empty packet from %s channelID=%d -> ACK", s.agentId, channelID)
			} else {
				delete(s.channels, channelID)
				callback = s.manager.cb.onClose
				logs.Debug("", "[WS-Agent] Second empty packet from %s channelID=%d -> Close", s.agentId, channelID)
			}
			s.channelsMu.Unlock()

			if callback != nil {
				callback(s.agentId, channelID)
			}
			continue
		}

		channelState.acked = true
		s.channelsMu.Unlock()

		if cb := s.manager.cb.onData; cb != nil {
			logs.Debug("", "[WS-Agent] Data packet from %s channelID=%d -> Data callback", s.agentId, channelID)
			dataCopy := append([]byte(nil), payload...)
			cb(s.agentId, channelID, dataCopy)
		}
	}
}

func (s *wsAgentSession) close() {
	s.closeOnce.Do(func() {
		close(s.done)
		_ = s.conn.Close()
		s.manager.remove(s.agentId, s)
	})
}
