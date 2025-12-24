package playbook

import (
	"encoding/json"
	"sync"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"github.com/gorilla/websocket"
)

// EventSubscriber 事件订阅器
type EventSubscriber struct {
	wsURL          string
	conn           *websocket.Conn
	triggerManager *TriggerManager
	stopCh         chan struct{}
	reconnectDelay time.Duration
	mu             sync.Mutex
	running        bool
}

// NewEventSubscriber 创建事件订阅器
func NewEventSubscriber(wsURL string, tm *TriggerManager) *EventSubscriber {
	return &EventSubscriber{
		wsURL:          wsURL,
		triggerManager: tm,
		stopCh:         make(chan struct{}),
		reconnectDelay: 5 * time.Second,
	}
}

// Start 启动事件订阅
func (es *EventSubscriber) Start() error {
	es.mu.Lock()
	if es.running {
		es.mu.Unlock()
		return nil
	}
	es.running = true
	es.mu.Unlock()

	go es.subscribeLoop()
	utils.InfoLogger.Println("📡 Event subscriber started")
	return nil
}

// Stop 停止事件订阅
func (es *EventSubscriber) Stop() {
	es.mu.Lock()
	defer es.mu.Unlock()

	if !es.running {
		return
	}

	es.running = false
	close(es.stopCh)

	if es.conn != nil {
		es.conn.Close()
	}

	utils.InfoLogger.Println("📡 Event subscriber stopped")
}

// subscribeLoop 订阅循环
func (es *EventSubscriber) subscribeLoop() {
	for {
		select {
		case <-es.stopCh:
			return
		default:
		}

		if err := es.connect(); err != nil {
			utils.ErrorLogger.Printf("❌ Failed to connect for events: %v", err)
			time.Sleep(es.reconnectDelay)
			continue
		}

		es.readEvents()

		// 连接断开，等待重连
		time.Sleep(es.reconnectDelay)
	}
}

// connect 连接 WebSocket
func (es *EventSubscriber) connect() error {
	es.mu.Lock()
	defer es.mu.Unlock()

	if es.conn != nil {
		es.conn.Close()
	}

	conn, _, err := websocket.DefaultDialer.Dial(es.wsURL+"/events", nil)
	if err != nil {
		return err
	}

	es.conn = conn

	// 发送订阅消息
	subscribeMsg := map[string]interface{}{
		"action": "subscribe",
		"events": []string{
			string(EventAgentOnline),
			string(EventAgentOffline),
			string(EventCredsFound),
			string(EventDownloadComplete),
			string(EventTaskComplete),
		},
	}

	if err := conn.WriteJSON(subscribeMsg); err != nil {
		return err
	}

	utils.InfoLogger.Println("✅ Connected to event stream")
	return nil
}

// readEvents 读取事件
func (es *EventSubscriber) readEvents() {
	for {
		select {
		case <-es.stopCh:
			return
		default:
		}

		es.mu.Lock()
		conn := es.conn
		es.mu.Unlock()

		if conn == nil {
			return
		}

		_, message, err := conn.ReadMessage()
		if err != nil {
			utils.ErrorLogger.Printf("❌ Event read error: %v", err)
			return
		}

		es.handleMessage(message)
	}
}

// handleMessage 处理消息
func (es *EventSubscriber) handleMessage(message []byte) {
	var rawEvent struct {
		Type string                 `json:"type"`
		Data map[string]interface{} `json:"data"`
	}

	if err := json.Unmarshal(message, &rawEvent); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to parse event: %v", err)
		return
	}

	event := Event{
		Type:      EventType(rawEvent.Type),
		Timestamp: time.Now(),
		Data:      rawEvent.Data,
	}

	utils.DebugLogger.Printf("📨 Received event: %s", event.Type)

	// 交给触发器管理器处理
	es.triggerManager.HandleEvent(event)
}

// IsRunning 检查是否运行中
func (es *EventSubscriber) IsRunning() bool {
	es.mu.Lock()
	defer es.mu.Unlock()
	return es.running
}

// SimulateEvent 模拟事件（用于测试）
func (es *EventSubscriber) SimulateEvent(event Event) {
	utils.InfoLogger.Printf("🔧 Simulating event: %s", event.Type)
	es.triggerManager.HandleEvent(event)
}
