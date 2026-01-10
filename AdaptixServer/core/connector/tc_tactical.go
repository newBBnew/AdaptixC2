package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

type TacticalWorkflowUpdate struct {
	Steps        []TacticalWorkflowStep `json:"steps"`
	TargetAgents string                 `json:"target_agents"`
}

func (tc *TsConnector) TcTacticalWorkflowUpdate(ctx *gin.Context) {
	var update TacticalWorkflowUpdate

	err := ctx.ShouldBindJSON(&update)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tactical workflow update"))
		return
	}

	tc.teamserver.TsTacticalWorkflowUpdate(update.Steps, update.TargetAgents)

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcTacticalWorkflowClear(ctx *gin.Context) {
	tc.teamserver.TsTacticalWorkflowClear()

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}

type TacticalLibraryUpdate struct {
	Category string        `json:"category"`
	Block    TacticalBlock `json:"block"`
}

func (tc *TsConnector) TcTacticalLibraryUpdate(ctx *gin.Context) {
	var update TacticalLibraryUpdate

	err := ctx.ShouldBindJSON(&update)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tactical library update"))
		return
	}

	err = tc.teamserver.TsTacticalLibraryUpdate(update.Category, update.Block)
	if err != nil {
		_ = ctx.Error(err)
		return
	}

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}

type TacticalLibraryDelete struct {
	BlockId string `json:"block_id"`
}

func (tc *TsConnector) TcTacticalLibraryDelete(ctx *gin.Context) {
	var del TacticalLibraryDelete

	err := ctx.ShouldBindJSON(&del)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tactical library delete"))
		return
	}

	err = tc.teamserver.TsTacticalLibraryDelete(del.BlockId)
	if err != nil {
		_ = ctx.Error(err)
		return
	}

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}

type TacticalAiSuggestion struct {
	Content string `json:"content"`
}

func (tc *TsConnector) TcTacticalAiSuggestion(ctx *gin.Context) {
	var suggestion TacticalAiSuggestion

	err := ctx.ShouldBindJSON(&suggestion)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tactical ai suggestion"))
		return
	}

	tc.teamserver.TsTacticalAiSuggestion(suggestion.Content)

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}
