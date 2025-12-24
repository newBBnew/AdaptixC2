package playbook

import (
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"
)

// ActionTransport 定义 action 的传输方式
type ActionTransport struct {
	Type    string `yaml:"type"`    // "mcp_tool" or "internal"
	Tool    string `yaml:"tool"`    // MCP tool name (for mcp_tool)
	Handler string `yaml:"handler"` // Internal handler name (for internal)
}

// ActionDef 定义单个 action
type ActionDef struct {
	Description string                 `yaml:"description"`
	Transport   ActionTransport        `yaml:"transport"`
	UI          map[string]interface{} `yaml:"ui"`
}

// ActionCatalogDoc Action Catalog 文档结构
type ActionCatalogDoc struct {
	APIVersion string                 `yaml:"apiVersion"`
	Kind       string                 `yaml:"kind"`
	Metadata   map[string]interface{} `yaml:"metadata"`
	Actions    map[string]ActionDef   `yaml:"actions"`
}

// ActionCatalog 管理加载的 actions
type ActionCatalog struct {
	actions map[string]ActionDef
}

// NewActionCatalog 创建新的 ActionCatalog
func NewActionCatalog() *ActionCatalog {
	return &ActionCatalog{
		actions: make(map[string]ActionDef),
	}
}

// LoadActionCatalog 从工作空间加载 Action Catalog
func LoadActionCatalog() (*ActionCatalog, error) {
	catalog := NewActionCatalog()

	// 获取 catalog 目录
	wsDir, err := WorkspaceDir()
	if err != nil {
		return nil, fmt.Errorf("failed to get workspace dir: %w", err)
	}

	catalogDir := filepath.Join(wsDir, "catalog")

	// 加载 actions.yaml
	actionsPath := filepath.Join(catalogDir, "actions.yaml")
	if err := catalog.loadFromFile(actionsPath); err != nil {
		// 文件不存在不算错误
		if !os.IsNotExist(err) {
			return nil, fmt.Errorf("failed to load actions.yaml: %w", err)
		}
	}

	return catalog, nil
}

// loadFromFile 从文件加载 actions
func (c *ActionCatalog) loadFromFile(path string) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return err
	}

	var doc ActionCatalogDoc
	if err := yaml.Unmarshal(data, &doc); err != nil {
		return fmt.Errorf("failed to parse %s: %w", path, err)
	}

	// 合并 actions
	for id, action := range doc.Actions {
		c.actions[id] = action
	}

	return nil
}

// ResolveAction 解析 action ID 到定义
func (c *ActionCatalog) ResolveAction(actionID string) (*ActionDef, error) {
	action, ok := c.actions[actionID]
	if !ok {
		return nil, fmt.Errorf("action not found: %s", actionID)
	}
	return &action, nil
}

// GetToolName 获取 action 对应的 MCP tool 名称
func (c *ActionCatalog) GetToolName(actionID string) (string, error) {
	action, err := c.ResolveAction(actionID)
	if err != nil {
		return "", err
	}

	if action.Transport.Type == "internal" {
		return "", fmt.Errorf("action %s is internal, use GetHandler instead", actionID)
	}

	if action.Transport.Type != "mcp_tool" {
		return "", fmt.Errorf("action %s has unsupported transport type: %s", actionID, action.Transport.Type)
	}

	if action.Transport.Tool == "" {
		return "", fmt.Errorf("action %s has no tool specified", actionID)
	}

	return action.Transport.Tool, nil
}

// IsInternalAction 检查 action 是否为内部处理
func (c *ActionCatalog) IsInternalAction(actionID string) bool {
	action, err := c.ResolveAction(actionID)
	if err != nil {
		return false
	}
	return action.Transport.Type == "internal"
}

// GetHandler 获取内部 action 的处理器名称
func (c *ActionCatalog) GetHandler(actionID string) (string, error) {
	action, err := c.ResolveAction(actionID)
	if err != nil {
		return "", err
	}

	if action.Transport.Type != "internal" {
		return "", fmt.Errorf("action %s is not internal", actionID)
	}

	if action.Transport.Handler == "" {
		return "", fmt.Errorf("action %s has no handler specified", actionID)
	}

	return action.Transport.Handler, nil
}

// IsDangerousAction 检查 action 是否为高危操作
func (c *ActionCatalog) IsDangerousAction(actionID string) bool {
	action, err := c.ResolveAction(actionID)
	if err != nil {
		return true // 未知 action 视为高危
	}

	if danger, ok := action.UI["danger"].(string); ok {
		return danger == "high"
	}

	return false
}

// ListActions 列出所有已注册的 actions
func (c *ActionCatalog) ListActions() []string {
	result := make([]string, 0, len(c.actions))
	for id := range c.actions {
		result = append(result, id)
	}
	return result
}
