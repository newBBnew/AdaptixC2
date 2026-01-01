package connector

import (
	"io"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcFileDeliveryUpload(ctx *gin.Context) {
	username := ctx.GetString("username")
	fileName := ctx.PostForm("file_name")
	file, err := ctx.FormFile("file")
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "file is required", "ok": false})
		return
	}

	f, err := file.Open()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}
	defer f.Close()

	fileData, err := io.ReadAll(f)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	fd, err := tc.teamserver.TsFileDeliveryUpload(username, fileName, fileData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "File uploaded", "ok": true, "data": fd})
}

func (tc *TsConnector) TcFileDeliveryList(ctx *gin.Context) {
	username := ctx.GetString("username")
	files, err := tc.teamserver.TsFileDeliveryList(username)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"ok": true, "data": files})
}

func (tc *TsConnector) TcFileDeliveryDelete(ctx *gin.Context) {
	username := ctx.GetString("username")
	var req struct {
		FileID string `json:"file_id"`
	}
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid request", "ok": false})
		return
	}

	err := tc.teamserver.TsFileDeliveryDelete(username, req.FileID)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "File deleted", "ok": true})
}

func (tc *TsConnector) TcFileDeliveryCreateLink(ctx *gin.Context) {
	username := ctx.GetString("username")
	var req struct {
		FileID      string `json:"file_id"`
		ExpireHours int    `json:"expire_hours"`
		MaxUses     int    `json:"max_uses"`
		AllowedIP   string `json:"allowed_ip"`
	}
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid request", "ok": false})
		return
	}

	token, url, err := tc.teamserver.TsFileDeliveryCreateLink(username, req.FileID, req.ExpireHours, req.MaxUses, req.AllowedIP)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"ok": true, "token": token, "url": url})
}

func (tc *TsConnector) TcFileDeliveryDownload(ctx *gin.Context) {
	token := ctx.Param("token")
	clientIP := ctx.ClientIP()

	path, fileName, _, err := tc.teamserver.TsFileDeliveryResolveToken(token, clientIP)
	if err != nil {
		ctx.String(http.StatusForbidden, err.Error())
		return
	}

	ctx.FileAttachment(path, fileName)
}
