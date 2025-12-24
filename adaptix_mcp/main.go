package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"syscall"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/mcp"
	"github.com/adaptix/adaptix_mcp/pkg/playbook"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"gopkg.in/yaml.v3"
)

var lockFile *os.File

func acquireSingletonLock() bool {
	home, err := os.UserHomeDir()
	if err != nil {
		return true
	}
	lockPath := filepath.Join(home, ".adaptix", "adaptix_mcp.lock")
	_ = os.MkdirAll(filepath.Dir(lockPath), 0o755)
	f, err := os.OpenFile(lockPath, os.O_CREATE|os.O_RDWR, 0o644)
	if err != nil {
		return true
	}
	if err := syscall.Flock(int(f.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		_ = f.Close()
		return false
	}
	_, _ = f.Seek(0, 0)
	_ = f.Truncate(0)
	_, _ = f.WriteString(fmt.Sprintf("pid=%d\n", os.Getpid()))
	lockFile = f
	return true
}

// Config 配置结构体
type Config struct {
	Client struct {
		URL               string        `yaml:"url"`
		ReconnectInterval time.Duration `yaml:"reconnect_interval"`
		Timeout           time.Duration `yaml:"timeout"`
	} `yaml:"client"`
	MCP struct {
		Name    string `yaml:"name"`
		Version string `yaml:"version"`
	} `yaml:"mcp"`
	Logging struct {
		Level  string `yaml:"level"`
		Format string `yaml:"format"`
	} `yaml:"logging"`
}

// loadConfig 加载配置文件
func loadConfig(configPath string) (*Config, error) {
	config := &Config{}

	// 设置默认值
	config.Client.URL = "ws://127.0.0.1:9999"
	config.Client.ReconnectInterval = 5 * time.Second
	config.Client.Timeout = 30 * time.Second
	config.MCP.Name = "adaptix-mcp"
	config.MCP.Version = "1.0.0"
	config.Logging.Level = "info"
	config.Logging.Format = "text"

	// 如果配置文件存在，则加载
	if configPath != "" {
		file, err := os.ReadFile(configPath)
		if err != nil {
			if !os.IsNotExist(err) {
				return nil, err
			}
			utils.WarnLogger.Printf("Config file not found: %s, using defaults", configPath)
			return config, nil
		}

		if err := yaml.Unmarshal(file, config); err != nil {
			return nil, err
		}

		utils.InfoLogger.Printf("✅ Loaded config from: %s", configPath)
	}

	return config, nil
}

func main() {
	if !acquireSingletonLock() {
		return
	}

	// 命令行参数
	configPath := flag.String("config", "configs/default.yaml", "Path to config file")
	clientURL := flag.String("url", "", "Client MCP Bridge URL (overrides config)")
	flag.Parse()

	// 加载配置
	config, err := loadConfig(*configPath)
	if err != nil {
		utils.ErrorLogger.Printf("❌ Failed to load config: %v", err)
		os.Exit(1)
	}

	// 命令行参数覆盖配置文件
	if *clientURL != "" {
		config.Client.URL = *clientURL
	}

	utils.InfoLogger.Println("🚀 Starting AdaptixC2 MCP Server...")
	utils.InfoLogger.Printf("📡 Client URL: %s", config.Client.URL)
	utils.InfoLogger.Printf("⚙️  Reconnect Interval: %v, Timeout: %v", config.Client.ReconnectInterval, config.Client.Timeout)

	// 创建Client连接器（使用配置）
	connector := client.NewConnector(config.Client.URL)
	connector.SetReconnectInterval(config.Client.ReconnectInterval)
	connector.SetTimeout(config.Client.Timeout)

	if err := playbook.EnsureWorkspace(); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to initialize playbook workspace: %v", err)
	}

	// 创建MCP Server
	server := mcp.NewMCPServer(connector)

	// 设置触发器执行器并加载规则
	playbook.SetTriggerExecutor(server)
	if err := playbook.GetTriggerManager().LoadRules(); err != nil {
		utils.ErrorLogger.Printf("⚠️ Failed to load trigger rules: %v", err)
	}

	// 启动Server
	if err := server.Start(); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to start MCP Server: %v", err)
		os.Exit(1)
	}
}
