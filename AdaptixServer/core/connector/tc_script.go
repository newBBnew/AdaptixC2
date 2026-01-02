package connector

import (
	"encoding/base64"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/gin-gonic/gin"
)

type ScriptReadRequest struct {
	Path string `json:"path"`
}

type ScriptInfo struct {
	Name  string `json:"name"`
	Path  string `json:"path"`
	IsDir bool   `json:"is_dir"`
	Size  int64  `json:"size"`
}

// TcScriptGetBasePath returns the configured extension path
func (tc *TsConnector) TcScriptGetBasePath(ctx *gin.Context) {
	basePath := tc.teamserver.TsGetExtensionPath()
	ctx.JSON(http.StatusOK, gin.H{
		"ok":   true,
		"path": basePath,
	})
}

// TcScriptList lists available scripts in the extension directory
func (tc *TsConnector) TcScriptList(ctx *gin.Context) {
	basePath := tc.teamserver.TsGetExtensionPath()
	if basePath == "" {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": "extension_path not configured in profile",
		})
		return
	}

	subPath := ctx.Query("path")
	targetPath := basePath
	if subPath != "" {
		targetPath = filepath.Join(basePath, subPath)
	}

	// Security check: ensure path is within basePath
	absTarget, err := filepath.Abs(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "invalid path"})
		return
	}
	absBase, _ := filepath.Abs(basePath)
	if !strings.HasPrefix(absTarget, absBase) {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "path traversal not allowed"})
		return
	}

	entries, err := os.ReadDir(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": err.Error(),
		})
		return
	}

	var scripts []ScriptInfo
	for _, entry := range entries {
		info, _ := entry.Info()
		scripts = append(scripts, ScriptInfo{
			Name:  entry.Name(),
			Path:  filepath.Join(subPath, entry.Name()),
			IsDir: entry.IsDir(),
			Size:  info.Size(),
		})
	}

	ctx.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"scripts": scripts,
		"base":    basePath,
	})
}

// TcScriptRead reads script content from the extension directory
func (tc *TsConnector) TcScriptRead(ctx *gin.Context) {
	var req ScriptReadRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "invalid request"})
		return
	}

	basePath := tc.teamserver.TsGetExtensionPath()
	if basePath == "" {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": "extension_path not configured in profile",
		})
		return
	}

	targetPath := filepath.Join(basePath, req.Path)

	// Security check: ensure path is within basePath
	absTarget, err := filepath.Abs(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "invalid path"})
		return
	}
	absBase, _ := filepath.Abs(basePath)
	if !strings.HasPrefix(absTarget, absBase) {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "path traversal not allowed"})
		return
	}

	content, err := ioutil.ReadFile(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": err.Error(),
		})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"path":    req.Path,
		"content": string(content),
	})
}

// TcScriptReadBof reads BOF binary file and returns base64 encoded content
func (tc *TsConnector) TcScriptReadBof(ctx *gin.Context) {
	var req ScriptReadRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "invalid request"})
		return
	}

	basePath := tc.teamserver.TsGetExtensionPath()
	if basePath == "" {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": "extension_path not configured in profile",
		})
		return
	}

	targetPath := filepath.Join(basePath, req.Path)

	// Security check
	absTarget, err := filepath.Abs(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "invalid path"})
		return
	}
	absBase, _ := filepath.Abs(basePath)
	if !strings.HasPrefix(absTarget, absBase) {
		ctx.JSON(http.StatusOK, gin.H{"ok": false, "message": "path traversal not allowed"})
		return
	}

	content, err := ioutil.ReadFile(targetPath)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{
			"ok":      false,
			"message": err.Error(),
		})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"path":    req.Path,
		"content": base64.StdEncoding.EncodeToString(content),
	})
}
