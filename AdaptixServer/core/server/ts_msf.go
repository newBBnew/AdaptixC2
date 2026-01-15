package server

import (
	"AdaptixServer/core/msf"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"strings"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type MSFModule struct {
	ts             *Teamserver
	api            *msf.API
	wsHandler      *msf.WSHandler
	config         *msf.MSFConfig
	controllerPath string
	mu             sync.RWMutex
}

var (
	msfModule     *MSFModule
	msfModuleOnce sync.Once
)

func NewMSFModule(ts *Teamserver, controllerPath string) *MSFModule {
	// Default config
	config := &msf.MSFConfig{
		Host:     "127.0.0.1",
		Port:     55552,
		User:     "msf",
		Password: "test123",
		SSL:      true,
	}
	api := msf.NewAPI(config, controllerPath)
	return &MSFModule{
		ts:             ts,
		api:            api,
		wsHandler:      msf.NewWSHandler(api),
		config:         config,
		controllerPath: controllerPath,
	}
}

func (m *MSFModule) Init() {
	logs.Info("msf", "MSF module initialized with controller: %s", m.controllerPath)
}

func (m *MSFModule) UpdateConfig(config *msf.MSFConfig) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.config = config
	m.api.UpdateConfig(config)
	logs.Info("msf", "MSF config updated: %s:%d", config.Host, config.Port)
}

func (m *MSFModule) GetConfig() *msf.MSFConfig {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.config
}

func (m *MSFModule) RegisterRoutes(r *gin.RouterGroup) {
	logs.Info("msf", "RegisterRoutes called with base path: %s", r.BasePath())

	m.api.RegisterRoutes(r)

	// 测试路由 - 不需要认证
	msf := r.Group("/msf")
	{
		msf.GET("/test", func(c *gin.Context) {
			logs.Info("msf", "Test route called!")
			c.JSON(http.StatusOK, gin.H{"status": "ok", "message": "Test route works"})
		})

		msf.GET("/debug", func(c *gin.Context) {
			logs.Info("msf", "Debug route called!")
			routes := m.collectRoutes(c)
			c.JSON(http.StatusOK, gin.H{
				"status":    "ok",
				"base_path": r.BasePath(),
				"routes":    routes,
			})
		})

		logs.Info("msf", "Registering /msf/config POST")
		msf.POST("/config", m.handleConfigUpdate)
		logs.Info("msf", "Registering /msf/controller/start POST")
		msf.POST("/controller/start", m.handleControllerStart)
		logs.Info("msf", "Registering /msf/controller/stop POST")
		msf.POST("/controller/stop", m.handleControllerStop)
		logs.Info("msf", "Registering /msf/controller/status GET")
		msf.GET("/controller/status", m.handleControllerStatus)
	}

	logs.Info("msf", "All MSF routes registered")
}

func (m *MSFModule) collectRoutes(c *gin.Context) []string {
	var routes []string
	routes = append(routes, fmt.Sprintf("FullPath: %s", c.Request.URL.Path))
	return routes
}

func (m *MSFModule) handleConfigUpdate(c *gin.Context) {
	var config msf.MSFConfig
	if err := c.ShouldBindJSON(&config); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"ok":    false,
			"error": "Invalid config",
		})
		return
	}

	m.UpdateConfig(&config)
	c.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"message": "Config updated",
	})
}

func (m *MSFModule) handleControllerStart(c *gin.Context) {
	logs.Info("msf", "=== handleControllerStart called ===")
	logs.Info("msf", "Controller path: %s", m.controllerPath)
	logs.Info("msf", "Full URL: %s %s", c.Request.Method, c.Request.URL.String())
	logs.Info("msf", "Content-Type: %s", c.ContentType())
	logs.Info("msf", "User: %s", c.GetString("username"))

	// 从请求体读取配置参数
	var config msf.MSFConfig
	if err := c.ShouldBindJSON(&config); err != nil {
		// 如果没有请求体，使用当前配置
		config = *m.GetConfig()
		logs.Info("msf", "No config in request, using current config: host=%s, port=%d",
			config.Host, config.Port)
	} else {
		// 更新配置
		m.UpdateConfig(&config)
		logs.Info("msf", "Config from request: host=%s, port=%d, user=%s, ssl=%v",
			config.Host, config.Port, config.User, config.SSL)
	}

	// 检查脚本是否存在
	if _, err := os.Stat(m.controllerPath); os.IsNotExist(err) {
		logs.Error("msf", "Controller script not found: %s", m.controllerPath)
		c.JSON(http.StatusInternalServerError, gin.H{
			"ok":    false,
			"error": "Controller script not found",
		})
		return
	}
	logs.Info("msf", "Controller script exists")

	// 构建命令参数
	args := []string{"start"}
	if config.Host != "" {
		args = append(args, "--host", config.Host)
	}
	if config.Port != 0 {
		args = append(args, "--port", fmt.Sprintf("%d", config.Port))
	}
	if config.User != "" {
		args = append(args, "--user", config.User)
	}
	if config.Password != "" {
		args = append(args, "--password", config.Password)
	}
	args = append(args, "--ssl", fmt.Sprintf("%t", config.SSL))

	// 执行脚本
	logs.Info("msf", "Executing: %s %s", m.controllerPath, strings.Join(args, " "))

	cmd := exec.Command(m.controllerPath, args...)
	output, err := cmd.CombinedOutput()

	logs.Info("msf", "Command output length: %d", len(output))
	logs.Info("msf", "Command error: %v", err)
	logs.Info("msf", "Raw output: %q", string(output))

	if err != nil {
		logs.Error("msf", "msfrpcd start failed: %v", err)
		logs.Error("msf", "Output: %s", string(output))
		c.JSON(http.StatusInternalServerError, gin.H{
			"ok":     false,
			"error":  err.Error(),
			"output": string(output),
		})
		return
	}

	logs.Info("msf", "msfrpcd started successfully")
	c.JSON(http.StatusOK, gin.H{
		"ok":     true,
		"output": string(output),
	})
}

func (m *MSFModule) handleControllerStop(c *gin.Context) {
	cmd := exec.Command(m.controllerPath, "stop")
	output, err := cmd.CombinedOutput()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"ok":     false,
			"error":  err.Error(),
			"output": string(output),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"ok":     true,
		"output": string(output),
	})
}

func (m *MSFModule) handleControllerStatus(c *gin.Context) {
	cmd := exec.Command(m.controllerPath, "status")
	output, err := cmd.Output()
	if err != nil {
		c.JSON(http.StatusOK, gin.H{
			"ok":     false,
			"status": "error",
			"output": string(output),
			"error":  err.Error(),
		})
		return
	}

	status := strings.TrimSpace(string(output))
	running := status == "running"

	c.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"status":  status,
		"running": running,
	})
}

func (m *MSFModule) Start() error {
	return m.api.Start()
}

func (m *MSFModule) Stop() {
	m.api.Stop()
}

func (m *MSFModule) GetStatus() msf.MSFStatus {
	return m.api.GetStatus()
}

func (m *MSFModule) GetAPI() *msf.API {
	return m.api
}

func (m *MSFModule) GetWSHandler() *msf.WSHandler {
	return m.wsHandler
}

type MSFWSServer struct {
	upgrader websocket.Upgrader
}

var msfgUpgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func (ts *Teamserver) TsMSFWebSocket(c *gin.Context) {
	userID := c.GetString("username")
	if userID == "" {
		userID = "anonymous"
	}

	conn, err := msfgUpgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		logs.Error("msf", "WebSocket upgrade error: %s", err.Error())
		return
	}

	if msfModule != nil {
		msfModule.wsHandler.HandleConnect(userID, conn)
	}
}

func (ts *Teamserver) InitMSF(config *msf.MSFConfig) {
	// 获取当前工作目录
	cwd, _ := os.Getwd()
	logs.Info("msf", "Current working directory: %s", cwd)

	// 使用相对路径，Server 从 release 目录启动时 scripts 在同级目录
	controllerPath := "./scripts/msf-controller"
	if _, err := os.Stat(controllerPath); os.IsNotExist(err) {
		controllerPath = "./scripts/msf-controller.sh"
	}

	// 尝试绝对路径
	if _, err := os.Stat(controllerPath); os.IsNotExist(err) {
		absPath := cwd + "/scripts/msf-controller"
		if _, err := os.Stat(absPath); err == nil {
			controllerPath = absPath
			logs.Info("msf", "Using absolute path: %s", controllerPath)
		}
	}

	logs.Info("msf", "MSF controller path: %s", controllerPath)

	msfModule = NewMSFModule(ts, controllerPath)
	msfModule.Init()

	if ts.AdaptixServer != nil && ts.AdaptixServer.Engine != nil {
		endpoint := ts.AdaptixServer.Endpoint
		apiGroup := ts.AdaptixServer.Engine.Group(endpoint + "/api")
		apiGroup.Use(token.ValidateAccessToken())
		logs.Info("msf", "Registering MSF routes under endpoint: %s with auth middleware", endpoint+"/api")
		{
			msfModule.RegisterRoutes(apiGroup)
			apiGroup.GET("/msf/ws", ts.TsMSFWebSocket)
		}
		logs.Info("msf", "MSF routes registered successfully")
	} else {
		logs.Error("msf", "AdaptixServer.Engine is nil, MSF routes not registered")
	}

	logs.Info("msf", "MSF module initialized")
}

func LoadMSFConfig(profilePath string) (*msf.MSFConfig, error) {
	data, err := os.ReadFile(profilePath)
	if err != nil {
		return nil, err
	}

	var profile struct {
		Teamserver struct {
			MSF *msf.MSFConfig `json:"msf"`
		} `json:"Teamserver"`
	}

	if err := json.Unmarshal(data, &profile); err != nil {
		return nil, err
	}

	return profile.Teamserver.MSF, nil
}
