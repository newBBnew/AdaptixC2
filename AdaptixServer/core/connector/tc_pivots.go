package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcPivotList(ctx *gin.Context) {
	pivots, err := tc.teamserver.TsPivotList()
	if err != nil {
		ctx.JSON(http.StatusInternalServerError, gin.H{"message": err.Error(), "ok": false})
		return
	}
	ctx.String(http.StatusOK, pivots)
}

func (tc *TsConnector) TcPivotRemove(ctx *gin.Context) {
	var req struct {
		PivotId string `json:"id"`
	}

	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}

	err := tc.teamserver.TsPivotDelete(req.PivotId)
	if err != nil {
		ctx.JSON(http.StatusInternalServerError, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Pivot removed", "ok": true})
}
