package playbook

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"gopkg.in/yaml.v3"
)

// EventType 事件类型
type EventType string

const (
	EventAgentOnline      EventType = "agent.online"
	EventAgentOffline     EventType = "agent.offline"
	EventCredsFound       EventType = "creds.found"
	EventDownloadComplete EventType = "download.complete"
	EventTaskComplete     EventType = "task.complete"
)

// Event 事件数据
type Event struct {
	Type      EventType              `json:"type"`
	Timestamp time.Time              `json:"timestamp"`
	Data      map[string]interface{} `json:"data"`
}

// TriggerFilter 触发过滤条件
type TriggerFilter struct {
	OS       string `yaml:"os" json:"os"`
	Hostname string `yaml:"hostname" json:"hostname"`
	Username string `yaml:"username" json:"username"`
	Tag      string `yaml:"tag" json:"tag"`
}

// TriggerRule 触发规则
type TriggerRule struct {
	ID         string                 `yaml:"id" json:"id"`
	Event      EventType              `yaml:"event" json:"event"`
	Filter     TriggerFilter          `yaml:"filter" json:"filter"`
	PlaybookID string                 `yaml:"playbook" json:"playbook"`
	Inputs     map[string]interface{} `yaml:"inputs" json:"inputs"`
	Delay      int                    `yaml:"delay" json:"delay"`   // 延迟执行秒数
	Dedupe     int                    `yaml:"dedupe" json:"dedupe"` // 去重窗口秒数
	Enabled    bool                   `yaml:"enabled" json:"enabled"`
}

// TriggerRulesDoc 触发规则文档
type TriggerRulesDoc struct {
	APIVersion string        `yaml:"apiVersion"`
	Kind       string        `yaml:"kind"`
	Rules      []TriggerRule `yaml:"rules"`
}

// TriggerManager 触发器管理器
type TriggerManager struct {
	rules       []TriggerRule
	dedupeCache map[string]time.Time // 去重缓存：key -> 上次触发时间
	mu          sync.RWMutex
	executor    TriggerExecutor
}

// TriggerExecutor 触发执行器接口
type TriggerExecutor interface {
	ExecutePlaybook(playbookID string, inputs map[string]interface{}) error
}

// 全局 TriggerManager 实例
var (
	globalTriggerManager *TriggerManager
	triggerManagerOnce   sync.Once
)

// GetTriggerManager 获取全局 TriggerManager 实例
func GetTriggerManager() *TriggerManager {
	triggerManagerOnce.Do(func() {
		globalTriggerManager = &TriggerManager{
			rules:       make([]TriggerRule, 0),
			dedupeCache: make(map[string]time.Time),
		}
	})
	return globalTriggerManager
}

// SetTriggerExecutor 设置触发执行器
func SetTriggerExecutor(executor TriggerExecutor) {
	tm := GetTriggerManager()
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.executor = executor
}

// NewTriggerManager 创建触发器管理器
func NewTriggerManager(executor TriggerExecutor) *TriggerManager {
	return &TriggerManager{
		rules:       make([]TriggerRule, 0),
		dedupeCache: make(map[string]time.Time),
		executor:    executor,
	}
}

// LoadRules 加载触发规则
func (tm *TriggerManager) LoadRules() error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	wsDir, err := WorkspaceDir()
	if err != nil {
		return err
	}

	// 加载 triggers.yaml
	triggersPath := filepath.Join(wsDir, "triggers.yaml")
	if data, err := os.ReadFile(triggersPath); err == nil {
		var doc TriggerRulesDoc
		if err := yaml.Unmarshal(data, &doc); err == nil {
			tm.rules = append(tm.rules, doc.Rules...)
		}
	}

	// 扫描 playbooks 中内嵌的 triggers
	playbooksDir := filepath.Join(wsDir, "playbooks")
	_ = filepath.WalkDir(playbooksDir, func(path string, d os.DirEntry, walkErr error) error {
		if walkErr != nil || d.IsDir() {
			return nil
		}
		if !strings.HasSuffix(strings.ToLower(d.Name()), ".yaml") {
			return nil
		}

		data, err := os.ReadFile(path)
		if err != nil {
			return nil
		}

		var doc struct {
			Metadata struct {
				ID string `yaml:"id"`
			} `yaml:"metadata"`
			Triggers []struct {
				Event  EventType     `yaml:"event"`
				Filter TriggerFilter `yaml:"filter"`
				Delay  int           `yaml:"delay"`
				Dedupe int           `yaml:"dedupe"`
			} `yaml:"triggers"`
		}

		if err := yaml.Unmarshal(data, &doc); err != nil {
			return nil
		}

		for i, t := range doc.Triggers {
			tm.rules = append(tm.rules, TriggerRule{
				ID:         fmt.Sprintf("%s_trigger_%d", doc.Metadata.ID, i),
				Event:      t.Event,
				Filter:     t.Filter,
				PlaybookID: doc.Metadata.ID,
				Inputs:     map[string]interface{}{},
				Delay:      t.Delay,
				Dedupe:     t.Dedupe,
				Enabled:    true,
			})
		}

		return nil
	})

	utils.InfoLogger.Printf("✅ Loaded %d trigger rules", len(tm.rules))
	return nil
}

// HandleEvent 处理事件
func (tm *TriggerManager) HandleEvent(event Event) {
	tm.mu.RLock()
	rules := make([]TriggerRule, len(tm.rules))
	copy(rules, tm.rules)
	tm.mu.RUnlock()

	for _, rule := range rules {
		if !rule.Enabled {
			continue
		}

		if rule.Event != event.Type {
			continue
		}

		if !tm.matchFilter(rule.Filter, event.Data) {
			continue
		}

		// 检查去重
		if rule.Dedupe > 0 {
			dedupeKey := tm.getDedupeKey(rule, event)
			tm.mu.Lock()
			lastTrigger, exists := tm.dedupeCache[dedupeKey]
			if exists && time.Since(lastTrigger) < time.Duration(rule.Dedupe)*time.Second {
				tm.mu.Unlock()
				utils.DebugLogger.Printf("⏭️ Skipping trigger %s (dedupe)", rule.ID)
				continue
			}
			tm.dedupeCache[dedupeKey] = time.Now()
			tm.mu.Unlock()
		}

		// 准备输入
		inputs := tm.expandInputs(rule.Inputs, event)

		// 延迟执行
		if rule.Delay > 0 {
			go func(r TriggerRule, i map[string]interface{}) {
				time.Sleep(time.Duration(r.Delay) * time.Second)
				tm.executePlaybook(r, i)
			}(rule, inputs)
		} else {
			go tm.executePlaybook(rule, inputs)
		}
	}
}

// matchFilter 匹配过滤条件
func (tm *TriggerManager) matchFilter(filter TriggerFilter, data map[string]interface{}) bool {
	if filter.OS != "" {
		os, _ := data["os"].(string)
		if !matchWildcard(filter.OS, os) {
			return false
		}
	}

	if filter.Hostname != "" {
		hostname, _ := data["hostname"].(string)
		if !matchWildcard(filter.Hostname, hostname) {
			return false
		}
	}

	if filter.Username != "" {
		username, _ := data["username"].(string)
		if !matchWildcard(filter.Username, username) {
			return false
		}
	}

	if filter.Tag != "" {
		tag, _ := data["tag"].(string)
		if !matchWildcard(filter.Tag, tag) {
			return false
		}
	}

	return true
}

// matchWildcard 通配符匹配
func matchWildcard(pattern, value string) bool {
	pattern = strings.ToLower(pattern)
	value = strings.ToLower(value)

	// 简单通配符转正则
	regexPattern := "^" + regexp.QuoteMeta(pattern) + "$"
	regexPattern = strings.ReplaceAll(regexPattern, `\*`, ".*")
	regexPattern = strings.ReplaceAll(regexPattern, `\?`, ".")

	matched, _ := regexp.MatchString(regexPattern, value)
	return matched
}

// getDedupeKey 生成去重 key
func (tm *TriggerManager) getDedupeKey(rule TriggerRule, event Event) string {
	// 基于规则ID和关键事件数据生成
	agentID, _ := event.Data["agent_id"].(string)
	return fmt.Sprintf("%s:%s", rule.ID, agentID)
}

// expandInputs 展开输入中的事件引用
func (tm *TriggerManager) expandInputs(inputs map[string]interface{}, event Event) map[string]interface{} {
	result := make(map[string]interface{})

	// 创建事件上下文
	ctx := NewExprContext()
	ctx.Vars["event"] = event.Data

	for key, value := range inputs {
		if strVal, ok := value.(string); ok {
			expanded, err := ExpandString(strVal, ctx)
			if err == nil {
				result[key] = expanded
			} else {
				result[key] = value
			}
		} else {
			result[key] = value
		}
	}

	// 默认添加 agent_id
	if _, exists := result["agent_id"]; !exists {
		if agentID, ok := event.Data["agent_id"].(string); ok {
			result["agent_id"] = agentID
		}
	}

	return result
}

// executePlaybook 执行 playbook
func (tm *TriggerManager) executePlaybook(rule TriggerRule, inputs map[string]interface{}) {
	utils.InfoLogger.Printf("🎯 Trigger %s firing: playbook=%s", rule.ID, rule.PlaybookID)

	if tm.executor == nil {
		utils.ErrorLogger.Printf("❌ No executor configured for trigger")
		return
	}

	if err := tm.executor.ExecutePlaybook(rule.PlaybookID, inputs); err != nil {
		utils.ErrorLogger.Printf("❌ Trigger execution failed: %v", err)
	}
}

// ListRules 列出所有规则
func (tm *TriggerManager) ListRules() []TriggerRule {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	rules := make([]TriggerRule, len(tm.rules))
	copy(rules, tm.rules)
	return rules
}

// SetRuleEnabled 启用/禁用规则
func (tm *TriggerManager) SetRuleEnabled(ruleID string, enabled bool) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	for i := range tm.rules {
		if tm.rules[i].ID == ruleID {
			tm.rules[i].Enabled = enabled
			return nil
		}
	}
	return fmt.Errorf("rule not found: %s", ruleID)
}

// CreateEventFromAgentData 从 agent 数据创建事件
func CreateEventFromAgentData(eventType EventType, agentData map[string]interface{}) Event {
	return Event{
		Type:      eventType,
		Timestamp: time.Now(),
		Data:      agentData,
	}
}

// SaveTriggersConfig 保存触发规则到文件
func SaveTriggersConfig(rules []TriggerRule) error {
	wsDir, err := WorkspaceDir()
	if err != nil {
		return err
	}

	doc := TriggerRulesDoc{
		APIVersion: "adaptix.triggers/v1",
		Kind:       "TriggerRules",
		Rules:      rules,
	}

	data, err := yaml.Marshal(doc)
	if err != nil {
		return err
	}

	return os.WriteFile(filepath.Join(wsDir, "triggers.yaml"), data, 0o644)
}

// EventToJSON 将事件转为 JSON
func EventToJSON(event Event) string {
	data, _ := json.Marshal(event)
	return string(data)
}
