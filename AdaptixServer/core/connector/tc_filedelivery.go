package connector

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

type FileDeliveryCreateLinkReq struct {
	FileID      string `json:"file_id"`
	ExpireHours int    `json:"expire_hours"`
	MaxUses     int    `json:"max_uses"`
	AllowedIP   string `json:"allowed_ip"`
}

func (tc *TsConnector) TcFileDeliveryUpload(ctx *gin.Context) {
	fileHeader, err := ctx.FormFile("file")
	if err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"ok": false, "message": "file is required"})
		return
	}
	f, err := fileHeader.Open()
	if err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"ok": false, "message": "cannot open file"})
		return
	}
	defer func() { _ = f.Close() }()

	data := make([]byte, fileHeader.Size)
	_, _ = f.Read(data)

	owner := ctx.GetString("username")
	fd, err := tc.teamserver.TsFileDeliveryUpload(owner, fileHeader.Filename, data)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": err.Error()})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "data": fd})
}

func (tc *TsConnector) TcFileDeliveryList(ctx *gin.Context) {
	owner := ctx.GetString("username")
	rows, err := tc.teamserver.TsFileDeliveryList(owner)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": err.Error()})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "data": rows})
}

func (tc *TsConnector) TcFileDeliveryDelete(ctx *gin.Context) {
	owner := ctx.GetString("username")
	fileID := ctx.Param("id")
	if fileID == "" {
		ctx.JSON(http.StatusBadRequest, gin.H{"ok": false, "message": "id is required"})
		return
	}
	if err := tc.teamserver.TsFileDeliveryDelete(owner, fileID); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": err.Error()})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true})
}

func (tc *TsConnector) TcFileDeliveryCreateLink(ctx *gin.Context) {
	var req FileDeliveryCreateLinkReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"ok": false, "message": "invalid json"})
		return
	}
	owner := ctx.GetString("username")
	token, url, err := tc.teamserver.TsFileDeliveryCreateLink(owner, req.FileID, req.ExpireHours, req.MaxUses, req.AllowedIP)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": err.Error()})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "data": gin.H{"token": token, "url": url}})
}

func (tc *TsConnector) TcFileDeliveryDownload(ctx *gin.Context) {
	token := ctx.Param("token")
	clientIP := ctx.ClientIP()

	path, filename, _, err := tc.teamserver.TsFileDeliveryResolveToken(token, clientIP)
	if err != nil {
		ctx.JSON(http.StatusForbidden, gin.H{"ok": false, "message": "forbidden"})
		return
	}

	ctx.Header("Content-Disposition", "attachment; filename=\""+filename+"\"")
	ctx.File(path)
}
