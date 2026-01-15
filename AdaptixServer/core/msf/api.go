package msf

import (
	"AdaptixServer/core/utils/logs"
	"encoding/json"
	"fmt"
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

type API struct {
	client          *MsfrpcClient
	consoleManager  *ConsoleManager
	wsHandler       *WSHandler
	controllerPath  string
	config          *MSFConfig
	status          MSFStatus
	statusMu        sync.RWMutex
	pollInterval    time.Duration
	lastHealthCheck time.Time
	lastHealthOK    bool
	healthMu        sync.Mutex
}

func NewAPI(config *MSFConfig, controllerPath string) *API {
	api := &API{
		client:         NewClient(config.Host, config.Port, config.SSL),
		consoleManager: NewConsoleManager(nil),
		controllerPath: controllerPath,
		config:         config,
		status: MSFStatus{
			Running:   false,
			Connected: false,
			Message:   "not started",
		},
		pollInterval: 500 * time.Millisecond,
		lastHealthOK: false,
	}
	api.wsHandler = NewWSHandler(api)
	return api
}

func (a *API) SetClient(client *MsfrpcClient) {
	a.client = client
	a.consoleManager = NewConsoleManager(client)
	a.wsHandler = NewWSHandler(a)
}

func (a *API) UpdateConfig(config *MSFConfig) {
	a.config = config
	a.client = NewClient(config.Host, config.Port, config.SSL)
	a.consoleManager = NewConsoleManager(a.client)
	logs.Info("msf", "API config updated: %s:%d", config.Host, config.Port)
}

func (a *API) GetStatus() MSFStatus {
	a.statusMu.RLock()
	defer a.statusMu.RUnlock()
	return a.status
}

func (a *API) setStatus(status MSFStatus) {
	a.statusMu.Lock()
	a.status = status
	a.statusMu.Unlock()
}

func (a *API) IsRunning() bool {
	a.statusMu.RLock()
	running := a.status.Running
	a.statusMu.RUnlock()
	return running
}

func (a *API) IsConnected() bool {
	a.healthMu.Lock()
	defer a.healthMu.Unlock()

	if time.Since(a.lastHealthCheck) < 3*time.Second {
		return a.lastHealthOK
	}

	a.lastHealthCheck = time.Now()
	if a.client == nil {
		a.lastHealthOK = false
		return false
	}

	if err := a.client.CheckConnection(); err != nil {
		logs.Warn("msf", "RPC health check failed: %v", err)
		a.lastHealthOK = false
		a.setStatus(MSFStatus{
			Running:   a.IsRunning(),
			Connected: false,
			Message:   "rpc disconnected",
		})
		return false
	}

	a.lastHealthOK = true
	a.setStatus(MSFStatus{
		Running:   a.IsRunning(),
		Connected: true,
		Message:   "connected",
	})
	return true
}

func (a *API) Start() error {
	a.setStatus(MSFStatus{
		Running: true,
		Message: "connecting...",
	})

	if err := a.connect(); err != nil {
		a.healthMu.Lock()
		a.lastHealthCheck = time.Now()
		a.lastHealthOK = false
		a.healthMu.Unlock()
		a.setStatus(MSFStatus{
			Running:   true,
			Connected: false,
			Message:   "connection failed: " + err.Error(),
		})
		return err
	}

	a.setStatus(MSFStatus{
		Running:   true,
		Connected: true,
		Message:   "connected",
	})

	a.healthMu.Lock()
	a.lastHealthCheck = time.Now()
	a.lastHealthOK = true
	a.healthMu.Unlock()

	return nil
}

func (a *API) Stop() {
	a.setStatus(MSFStatus{
		Running:   false,
		Connected: false,
		Message:   "stopped",
	})
}

func (a *API) connect() error {
	logs.Info("msf", "Attempting to connect to MSF RPC at %s:%d (SSL: %v)", a.config.Host, a.config.Port, a.config.SSL)
	logs.Info("msf", "Login credentials: user=%s, password=%s", a.config.User, a.config.Password[:4]+"...")

	// 增加连接重试机制
	maxRetries := 5
	retryInterval := 2 * time.Second

	for i := 0; i < maxRetries; i++ {
		if i > 0 {
			logs.Info("msf", "Connection retry %d/%d, waiting %v...", i, maxRetries, retryInterval)
			time.Sleep(retryInterval)
			retryInterval *= 2 // 指数退避
		}

		token, err := a.client.Login(a.config.User, a.config.Password)
		if err != nil {
			if i < maxRetries-1 {
				logs.Warn("msf", "MSF login attempt %d failed: %v", i+1, err)
				continue
			}
			logs.Error("msf", "MSF login failed after %d attempts: %v", maxRetries, err)
			return err
		}

		logs.Info("msf", "MSF login successful, token: %s", token[:8]+"...")
		return nil
	}

	return fmt.Errorf("failed to connect after %d attempts", maxRetries)
}

func (a *API) RegisterRoutes(r *gin.RouterGroup) {
	msf := r.Group("/msf")
	{
		msf.POST("/start", a.handleStart)
		msf.POST("/stop", a.handleStop)
		msf.GET("/status", a.handleStatus)
		msf.GET("/health", a.handleHealth)

		console := msf.Group("/console")
		{
			console.POST("/create", a.handleConsoleCreate)
			console.POST("/:id/write", a.handleConsoleWrite)
			console.GET("/:id/read", a.handleConsoleRead)
			console.POST("/:id/destroy", a.handleConsoleDestroy)
		}

		session := msf.Group("/sessions")
		{
			session.GET("", a.handleSessionList)
			session.POST("/:id/interact", a.handleSessionInteract)
			session.POST("/:id/kill", a.handleSessionKill)
		}

		job := msf.Group("/jobs")
		{
			job.GET("", a.handleJobList)
			job.POST("/:id/kill", a.handleJobKill)
		}

		module := msf.Group("/modules")
		{
			module.GET("", a.handleModuleList)
			module.GET("/search", a.handleModuleSearch)
			module.GET("/info", a.handleModuleInfo)
			module.GET("/options", a.handleModuleOptions)
			module.GET("/compatible_payloads", a.handleModuleCompatiblePayloads)
			module.POST("/execute", a.handleModuleExecute)
		}
	}
}

func (a *API) handleStart(c *gin.Context) {
	logs.Debug("msf", "→ [API] POST /msf/start")
	logs.Debug("msf", "→ [API] IsConnected: %v", a.IsConnected())

	if a.IsConnected() {
		logs.Debug("msf", "→ [API] Already connected, updating ConsoleManager")
		// 即使已连接，也需要确保 ConsoleManager 使用的是当前 authenticated client
		a.consoleManager = NewConsoleManager(a.client)
		logs.Debug("msf", "← [API] Already connected, token: %s...", maskString(a.client.GetToken()[:8]))
		c.JSON(http.StatusOK, gin.H{"ok": true, "message": "already connected"})
		return
	}

	if err := a.Start(); err != nil {
		logs.Error("msf", "← [API] Connect failed: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	a.consoleManager = NewConsoleManager(a.client)
	logs.Debug("msf", "← [API] Connected, ConsoleManager updated")
	c.JSON(http.StatusOK, gin.H{"ok": true, "message": "connected"})
}

func (a *API) handleStop(c *gin.Context) {
	a.Stop()
	c.JSON(http.StatusOK, gin.H{"ok": true, "message": "stopped"})
}

func (a *API) handleStatus(c *gin.Context) {
	_ = a.IsConnected()
	status := a.GetStatus()
	c.JSON(http.StatusOK, gin.H{
		"ok":        true,
		"running":   status.Running,
		"connected": status.Connected,
		"message":   status.Message,
	})
}

func (a *API) handleHealth(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"ok":      false,
			"healthy": false,
			"message": "MSF RPC not connected",
		})
		return
	}

	// 尝试简单的版本检查来验证连接
	if version, err := a.client.Version(); err != nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"ok":      false,
			"healthy": false,
			"message": fmt.Sprintf("MSF RPC health check failed: %v", err),
		})
		return
	} else {
		c.JSON(http.StatusOK, gin.H{
			"ok":      true,
			"healthy": true,
			"message": "MSF RPC is healthy",
			"version": version,
		})
	}
}

func (a *API) handleConsoleCreate(c *gin.Context) {
	logs.Debug("msf", "→ [API] POST /msf/console/create")
	logs.Debug("msf", "→ [API] User: %s", c.GetString("username"))

	// Debug: 检查 client 状态
	clientToken := ""
	if a.client != nil {
		clientToken = a.client.GetToken()
	}
	logs.Debug("msf", "→ [API] API client token: %s...", maskString(clientToken))
	logs.Debug("msf", "→ [API] API IsConnected: %v", a.IsConnected())
	logs.Debug("msf", "→ [API] ConsoleManager client: %v", a.consoleManager.client != nil)
	if a.consoleManager.client != nil {
		logs.Debug("msf", "→ [API] ConsoleManager token: %s...", maskString(a.consoleManager.client.GetToken()))
	}

	if !a.IsConnected() {
		logs.Error("msf", "← [API] Not connected to MSF")
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	userID := c.GetString("username")
	if userID == "" {
		userID = "anonymous"
	}

	console, err := a.consoleManager.Create(userID)
	if err != nil {
		logs.Error("msf", "← [API] Create failed: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	logs.Debug("msf", "← [API] Console created: id=%s", console.ID)

	// Register console ID to WSHandler for this user
	a.wsHandler.AddUserConsole(userID, console.ID)

	c.JSON(http.StatusOK, gin.H{"ok": true, "id": console.ID})
}

func (a *API) handleConsoleWrite(c *gin.Context) {
	consoleID := c.Param("id")
	logs.Debug("msf", "→ [API] POST /msf/console/%s/write", consoleID)

	if !a.IsConnected() {
		logs.Error("msf", "← [API] Not connected to MSF")
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	var req struct {
		Command string `json:"command"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		logs.Error("msf", "← [API] Invalid request: %v", err)
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "invalid request"})
		return
	}

	logs.Debug("msf", "→ [API] Command: %q", truncate(req.Command, 50))

	if err := a.consoleManager.Write(consoleID, req.Command); err != nil {
		logs.Error("msf", "← [API] Write failed: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	logs.Debug("msf", "← [API] Write success")
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (a *API) handleConsoleRead(c *gin.Context) {
	consoleID := c.Param("id")
	logs.Debug("msf", "→ [API] GET /msf/console/%s/read", consoleID)

	if !a.IsConnected() {
		logs.Error("msf", "← [API] Not connected to MSF")
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	data, busy, err := a.consoleManager.Read(consoleID)
	if err != nil {
		logs.Error("msf", "← [API] Read failed: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	logs.Debug("msf", "← [API] Read: busy=%v, data_len=%d", busy, len(data))
	c.JSON(http.StatusOK, gin.H{"ok": true, "data": data, "busy": busy})
}

func (a *API) handleModuleList(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	modules, err := a.client.ModulesList()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "modules": modules})
}

func (a *API) handleModuleSearch(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	query := c.Query("query")
	if query == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "query required"})
		return
	}

	modules, err := a.client.SearchModules(query)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "modules": modules})
}

func (a *API) handleModuleInfo(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	moduleType := c.Query("type")
	moduleName := c.Query("name")
	if moduleType == "" || moduleName == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "type and name required"})
		return
	}

	info, err := a.client.ModuleInfo(moduleType, moduleName)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "info": info})
}

func (a *API) handleModuleOptions(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	moduleType := c.Query("type")
	moduleName := c.Query("name")
	if moduleType == "" || moduleName == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "type and name required"})
		return
	}

	options, err := a.client.ModuleOptions(moduleType, moduleName)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "options": options})
}

func (a *API) handleModuleCompatiblePayloads(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	moduleType := c.Query("type")
	moduleName := c.Query("name")
	if moduleType == "" || moduleName == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "type and name required"})
		return
	}

	payloads, err := a.client.ModuleCompatiblePayloads(moduleType, moduleName)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "payloads": payloads})
}

func (a *API) handleModuleExecute(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	var req struct {
		Type    string                 `json:"type"`
		Name    string                 `json:"name"`
		Options map[string]interface{} `json:"options"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "invalid request"})
		return
	}
	if req.Type == "" || req.Name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "type and name required"})
		return
	}
	if req.Options == nil {
		req.Options = map[string]interface{}{}
	}

	result, err := a.client.ModuleExecute(req.Type, req.Name, req.Options)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "result": result})
}

func (a *API) handleConsoleDestroy(c *gin.Context) {
	consoleID := c.Param("id")
	if err := a.consoleManager.Destroy(consoleID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (a *API) handleSessionList(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	sessions, err := a.client.SessionList()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "sessions": sessions})
}

func (a *API) handleSessionInteract(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	sessionID := c.Param("id")
	var req struct {
		Command string `json:"command"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "invalid request"})
		return
	}

	if req.Command == "" {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "empty command"})
		return
	}

	// 向会话写入命令
	if _, err := a.client.SessionWrite(sessionID, req.Command+"\n"); err != nil {
		logs.Error("msf", "Failed to write to session %s: %v", sessionID, err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	// 读取会话响应
	data, err := a.client.SessionRead(sessionID)
	if err != nil {
		logs.Error("msf", "Failed to read from session %s: %v", sessionID, err)
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	responsePreview := data
	if len(responsePreview) > 100 {
		responsePreview = responsePreview[:100] + "..."
	}
	logs.Info("msf", "Session %s interaction: command='%s', response='%s'", sessionID, req.Command, responsePreview)

	c.JSON(http.StatusOK, gin.H{
		"ok":   true,
		"data": data,
	})
}

func (a *API) handleSessionKill(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	sessionID := c.Param("id")
	if err := a.client.SessionKill(sessionID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (a *API) handleJobList(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	jobs, err := a.client.JobsList()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "jobs": jobs})
}

func (a *API) handleJobKill(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	jobID := c.Param("id")
	if err := a.client.JobsKill(jobID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (a *API) GetClient() *MsfrpcClient {
	return a.client
}

func (a *API) GetConsoleManager() *ConsoleManager {
	return a.consoleManager
}

type MSFAPIResponse struct {
	Ok      bool        `json:"ok"`
	Data    interface{} `json:"data,omitempty"`
	Error   string      `json:"error,omitempty"`
	Message string      `json:"message,omitempty"`
}

func (a *API) SessionsToJSON(sessions map[string]SessionInfo) string {
	data, _ := json.Marshal(sessions)
	return string(data)
}

func (a *API) JobsToJSON(jobs map[string]JobInfo) string {
	data, _ := json.Marshal(jobs)
	return string(data)
}
