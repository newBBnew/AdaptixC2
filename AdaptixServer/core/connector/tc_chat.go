package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

type ChatMessage struct {
	Message string `json:"message"`
}

func (tc *TsConnector) TcChatSendMessage(ctx *gin.Context) {
	var chat_message ChatMessage

	err := ctx.ShouldBindJSON(&chat_message)
	if err != nil {
		_ = ctx.Error(errors.New("invalid message"))
		return
	}

	username := ctx.GetString("username")

	tc.teamserver.TsChatSendMessage(username, chat_message.Message)

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}

type SessionArchiveResponse struct {
	Ok        bool   `json:"ok"`
	SessionId string `json:"session_id"`
}

func (tc *TsConnector) TcSessionArchiveCurrent(ctx *gin.Context) {
	sessionId, err := tc.teamserver.TsSessionArchiveCurrent()
	if err != nil {
		_ = ctx.Error(err)
		return
	}

	answer := SessionArchiveResponse{
		Ok:        true,
		SessionId: sessionId,
	}
	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcSessionList(ctx *gin.Context) {
	sessions := tc.teamserver.TsSessionList()
	answer := gin.H{
		"ok":       true,
		"sessions": sessions,
	}
	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcSessionGetContent(ctx *gin.Context) {
	sessionId := ctx.Param("id")
	if sessionId == "" {
		_ = ctx.Error(http.ErrNoLocation) // reusing err for bad request
		return
	}

	messages := tc.teamserver.TsSessionGetContent(sessionId)
	answer := gin.H{
		"ok":       true,
		"messages": messages,
	}
	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcSessionDelete(ctx *gin.Context) {
	var params struct {
		SessionId string `json:"session_id"`
	}

	if err := ctx.ShouldBindJSON(&params); err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}

	if params.SessionId == "" {
		_ = ctx.Error(errors.New("missing session_id"))
		return
	}

	err := tc.teamserver.TsSessionDelete(params.SessionId)
	if err != nil {
		_ = ctx.Error(err)
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": "Session deleted"})
}
