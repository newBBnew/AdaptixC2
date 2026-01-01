package client

import (
	"fmt"
	"sync"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"github.com/gorilla/websocket"
)

type Request struct {
	RequestID string                 `json:"request_id"`
	Type      string                 `json:"type"`
	Params    map[string]interface{} `json:"params"`
}

type Response struct {
	RequestID string                 `json:"request_id"`
	Status    string                 `json:"status"`
	Message   string                 `json:"message"`
	Data      map[string]interface{} `json:"data"`
	Version   string                 `json:"version"`
}

type Connector struct {
	url     string
	conn    *websocket.Conn
	mu      sync.RWMutex
	pending sync.Map

	reconnectInterval time.Duration
	timeout           time.Duration

	connected    bool
	reconnecting bool
	stopChan     chan struct{}
}

func NewConnector(url string) *Connector {
	return &Connector{
		url:               url,
		reconnectInterval: 5 * time.Second,
		timeout:           30 * time.Second,
		stopChan:          make(chan struct{}),
	}
}

func (c *Connector) SetReconnectInterval(d time.Duration) { c.reconnectInterval = d }
func (c *Connector) SetTimeout(d time.Duration)           { c.timeout = d }
func (c *Connector) IsConnected() bool                    { return c.connected }

func (c *Connector) Connect() error {
	conn, _, err := websocket.DefaultDialer.Dial(c.url, nil)
	if err != nil {
		return fmt.Errorf("failed to connect to %s: %w", c.url, err)
	}

	c.conn = conn
	c.connected = true

	utils.InfoLogger.Printf("✅ Connected to Client MCP Bridge at %s", c.url)

	go c.listenResponses()

	return nil
}

func (c *Connector) Close() error {
	close(c.stopChan)
	c.connected = false

	if c.conn != nil {
		return c.conn.Close()
	}

	return nil
}

func (c *Connector) SendCommand(commandType string, params map[string]interface{}) (*Response, error) {
	c.mu.RLock()
	isConnected := c.connected
	c.mu.RUnlock()

	if !isConnected {
		if err := c.Connect(); err != nil {
			return nil, fmt.Errorf("not connected to Client MCP Bridge: %w", err)
		}
	}

	requestID := generateID()

	request := Request{
		RequestID: requestID,
		Type:      commandType,
		Params:    params,
	}

	respChan := make(chan *Response, 1)
	c.pending.Store(requestID, respChan)

	if err := c.conn.WriteJSON(request); err != nil {
		c.pending.Delete(requestID)
		return nil, fmt.Errorf("failed to send command: %w", err)
	}

	select {
	case resp := <-respChan:
		if resp.Status == "success" {
			return resp, nil
		}
		return nil, fmt.Errorf("command failed: %s", resp.Message)
	case <-time.After(c.timeout):
		c.pending.Delete(requestID)
		return nil, fmt.Errorf("command timeout after %v", c.timeout)
	}
}

func (c *Connector) listenResponses() {
	defer func() {
		c.mu.Lock()
		c.connected = false
		shouldReconnect := !c.reconnecting
		if shouldReconnect {
			c.reconnecting = true
		}
		c.mu.Unlock()

		if shouldReconnect {
			utils.WarnLogger.Println("Response listener stopped, will attempt reconnect...")
			go c.autoReconnect()
		}
	}()

	for {
		select {
		case <-c.stopChan:
			return
		default:
			var resp Response
			if err := c.conn.ReadJSON(&resp); err != nil {
				utils.ErrorLogger.Printf("Failed to read response: %v", err)
				return
			}

			if ch, ok := c.pending.Load(resp.RequestID); ok {
				if respChan, ok := ch.(chan *Response); ok {
					respChan <- &resp
				}
				c.pending.Delete(resp.RequestID)
			}
		}
	}
}

func (c *Connector) autoReconnect() {
	for {
		select {
		case <-c.stopChan:
			return
		case <-time.After(c.reconnectInterval):
			utils.InfoLogger.Println("Attempting to reconnect...")
			if err := c.Connect(); err != nil {
				utils.ErrorLogger.Printf("Reconnect failed: %v", err)
			} else {
				c.mu.Lock()
				c.reconnecting = false
				c.mu.Unlock()
				return
			}
		}
	}
}

var idCounter uint64
var idMu sync.Mutex

func generateID() string {
	idMu.Lock()
	defer idMu.Unlock()
	idCounter++
	return fmt.Sprintf("req_%d_%d", time.Now().UnixNano(), idCounter)
}
