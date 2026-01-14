package server

import (
	"AdaptixServer/core/msf"
	"AdaptixServer/core/utils/logs"
	"encoding/json"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
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

func NewMSFModule(ts *Teamserver, config *msf.MSFConfig, controllerPath string) *MSFModule {
	return &MSFModule{
		ts:             ts,
		api:            msf.NewAPI(config, controllerPath),
		wsHandler:      msf.NewWSHandler(nil),
		config:         config,
		controllerPath: controllerPath,
	}
}

func (m *MSFModule) Init() {
	m.api.SetClient(m.api.GetClient())
	m.api.GetConsoleManager()
	m.wsHandler.StartBackgroundMonitor()
	logs.Info("msf", "MSF module initialized")
}

func (m *MSFModule) RegisterRoutes(r *gin.RouterGroup) {
	m.api.RegisterRoutes(r)

	msf := r.Group("/msf")
	{
		msf.POST("/controller/start", m.handleControllerStart)
		msf.POST("/controller/stop", m.handleControllerStop)
		msf.GET("/controller/status", m.handleControllerStatus)
	}
}

func (m *MSFModule) handleControllerStart(c *gin.Context) {
	cmd := exec.Command(m.controllerPath, "start")
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
	if config == nil {
		logs.Warn("msf", "MSF config not found, skipping initialization")
		return
	}

	scriptDir, err := filepath.Abs("scripts")
	if err != nil {
		scriptDir = "scripts"
	}

	controllerPath := filepath.Join(scriptDir, "msf-controller")
	if _, err := exec.Command("test", "-x", controllerPath).Output(); err != nil {
		controllerPath = filepath.Join(scriptDir, "msf-controller.sh")
	}

	msfModule = NewMSFModule(ts, config, controllerPath)
	msfModule.Init()

	if ts.AdaptixServer != nil && ts.AdaptixServer.Engine != nil {
		apiGroup := ts.AdaptixServer.Engine.Group("/api")
		{
			msfModule.RegisterRoutes(apiGroup)
			apiGroup.GET("/msf/ws", ts.TsMSFWebSocket)
		}
	}

	logs.Info("msf", "MSF module registered")
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
