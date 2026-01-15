package msf

import (
	"fmt"
	"strings"
	"sync"
	"time"
)

type ConsoleManager struct {
	client       *MsfrpcClient
	consoleMap   map[string]*UserConsole
	userConsoles map[string][]string
	mu           sync.RWMutex
}

type UserConsole struct {
	ID         string
	UserID     string
	CreatedAt  time.Time
	LastActive time.Time
	LastPrompt string
}

func NewConsoleManager(client *MsfrpcClient) *ConsoleManager {
	return &ConsoleManager{
		client:       client,
		consoleMap:   make(map[string]*UserConsole),
		userConsoles: make(map[string][]string),
	}
}

func (cm *ConsoleManager) Create(userID string) (*UserConsole, error) {
	consoleResult, err := cm.client.ConsoleCreate()
	if err != nil {
		return nil, err
	}

	consoleID, ok := consoleResult["id"].(string)
	if !ok {
		return nil, fmt.Errorf("invalid console response")
	}

	console := &UserConsole{
		ID:         consoleID,
		UserID:     userID,
		CreatedAt:  time.Now(),
		LastActive: time.Now(),
	}

	cm.mu.Lock()
	cm.consoleMap[consoleID] = console
	cm.userConsoles[userID] = append(cm.userConsoles[userID], consoleID)
	cm.mu.Unlock()

	return console, nil
}

func (cm *ConsoleManager) Write(consoleID, command string) error {
	cm.mu.Lock()
	if console, ok := cm.consoleMap[consoleID]; ok {
		console.LastActive = time.Now()
	}
	cm.mu.Unlock()

	trimmed := strings.TrimSpace(command)
	if trimmed != "" && !strings.HasSuffix(command, "\n") {
		command += "\n"
	}

	_, err := cm.client.ConsoleWrite(consoleID, command)
	return err
}

func (cm *ConsoleManager) Read(consoleID string) (string, bool, error) {
	cm.mu.Lock()
	var lastPrompt string
	if console, ok := cm.consoleMap[consoleID]; ok {
		console.LastActive = time.Now()
		lastPrompt = console.LastPrompt
	}
	cm.mu.Unlock()

	result, err := cm.client.ConsoleRead(consoleID)
	if err != nil {
		return "", false, err
	}

	// 从map中提取数据
	data, _ := result["data"].(string)
	busy := false
	if b, ok := result["busy"].(bool); ok {
		busy = b
	}

	prompt, _ := result["prompt"].(string)
	if data == "" && prompt != "" && prompt != lastPrompt {
		data = prompt
	}

	if prompt != "" {
		cm.mu.Lock()
		if console, ok := cm.consoleMap[consoleID]; ok {
			console.LastPrompt = prompt
		}
		cm.mu.Unlock()
	}

	if data != "" || busy {
		return data, busy, nil
	}

	return "", false, nil
}

func (cm *ConsoleManager) Destroy(consoleID string) error {
	cm.mu.Lock()
	defer cm.mu.Unlock()

	if console, ok := cm.consoleMap[consoleID]; ok {
		delete(cm.consoleMap, consoleID)

		userConsoles := cm.userConsoles[console.UserID]
		for i, id := range userConsoles {
			if id == consoleID {
				cm.userConsoles[console.UserID] = append(userConsoles[:i], userConsoles[i+1:]...)
				break
			}
		}
	}

	return cm.client.ConsoleDestroy(consoleID)
}

func (cm *ConsoleManager) GetUserConsoles(userID string) []string {
	cm.mu.RLock()
	defer cm.mu.RUnlock()
	return cm.userConsoles[userID]
}

func (cm *ConsoleManager) GetConsole(consoleID string) *UserConsole {
	cm.mu.RLock()
	defer cm.mu.RUnlock()
	return cm.consoleMap[consoleID]
}

func (cm *ConsoleManager) GetAllConsoles() map[string]*UserConsole {
	cm.mu.RLock()
	defer cm.mu.RUnlock()
	return cm.consoleMap
}

func (cm *ConsoleManager) DestroyUserConsoles(userID string) error {
	cm.mu.Lock()
	consoleIDs := cm.userConsoles[userID]
	delete(cm.userConsoles, userID)
	cm.mu.Unlock()

	for _, consoleID := range consoleIDs {
		cm.Destroy(consoleID)
	}

	return nil
}

func (cm *ConsoleManager) PollConsole(consoleID string) (string, bool, error) {
	data, busy, err := cm.Read(consoleID)
	if err != nil {
		return "", false, err
	}

	cm.mu.Lock()
	if console, ok := cm.consoleMap[consoleID]; ok {
		console.LastActive = time.Now()
	}
	cm.mu.Unlock()

	return data, busy, nil
}
