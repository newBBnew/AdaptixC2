package main

import (
	"flag"
	"os"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/mcp"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"gopkg.in/yaml.v3"
)

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

	// 创建MCP Server
	server := mcp.NewMCPServer(connector)

	// 启动Server
	if err := server.Start(); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to start MCP Server: %v", err)
		os.Exit(1)
	}
}
