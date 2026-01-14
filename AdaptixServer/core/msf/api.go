package msf

import (
	"AdaptixServer/core/utils/logs"
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

type API struct {
	client         *MsfrpcClient
	consoleManager *ConsoleManager
	controllerPath string
	config         *MSFConfig
	status         MSFStatus
	statusMu       sync.RWMutex
	pollInterval   time.Duration
}

func NewAPI(config *MSFConfig, controllerPath string) *API {
	return &API{
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
	}
}

func (a *API) SetClient(client *MsfrpcClient) {
	a.client = client
	a.consoleManager = NewConsoleManager(client)
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
	a.statusMu.RLock()
	connected := a.status.Connected
	a.statusMu.RUnlock()
	return connected
}

func (a *API) Start() error {
	a.setStatus(MSFStatus{
		Running: true,
		Message: "connecting...",
	})

	if err := a.connect(); err != nil {
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
	token, err := a.client.Login(a.config.User, a.config.Password)
	if err != nil {
		return err
	}
	logs.Info("msf", "MSF login successful, token: %s", token[:8]+"...")
	return nil
}

func (a *API) RegisterRoutes(r *gin.RouterGroup) {
	msf := r.Group("/msf")
	{
		msf.POST("/start", a.handleStart)
		msf.POST("/stop", a.handleStop)
		msf.GET("/status", a.handleStatus)

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
	}
}

func (a *API) handleStart(c *gin.Context) {
	if a.IsConnected() {
		c.JSON(http.StatusOK, gin.H{"ok": true, "message": "already connected"})
		return
	}

	if err := a.Start(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "message": "connected"})
}

func (a *API) handleStop(c *gin.Context) {
	a.Stop()
	c.JSON(http.StatusOK, gin.H{"ok": true, "message": "stopped"})
}

func (a *API) handleStatus(c *gin.Context) {
	status := a.GetStatus()
	c.JSON(http.StatusOK, gin.H{
		"ok":        true,
		"running":   status.Running,
		"connected": status.Connected,
		"message":   status.Message,
	})
}

func (a *API) handleConsoleCreate(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	userID := c.GetString("username")
	if userID == "" {
		userID = "anonymous"
	}

	console, err := a.consoleManager.Create(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "id": console.ID})
}

func (a *API) handleConsoleWrite(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	consoleID := c.Param("id")
	var req struct {
		Command string `json:"command"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"ok": false, "error": "invalid request"})
		return
	}

	if err := a.consoleManager.Write(consoleID, req.Command); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func (a *API) handleConsoleRead(c *gin.Context) {
	if !a.IsConnected() {
		c.JSON(http.StatusServiceUnavailable, gin.H{"ok": false, "error": "not connected"})
		return
	}

	consoleID := c.Param("id")
	data, busy, err := a.consoleManager.Read(consoleID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"ok": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "data": data, "busy": busy})
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

	_ = req.Command
	_ = sessionID

	c.JSON(http.StatusOK, gin.H{"ok": true, "message": "session interact not implemented"})
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
