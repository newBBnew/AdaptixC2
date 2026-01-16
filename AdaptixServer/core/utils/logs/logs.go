package logs

import (
	"AdaptixServer/core/utils/tformat"
	"fmt"
	"sync"
	"time"
)

type PrintLogger struct {
	debug bool
}

var PrintLog *PrintLogger

const kServerDebugLogs = false

// LogBroadcaster 用于将日志广播到 WebSocket
var logBroadcaster struct {
	mu      sync.RWMutex
	send    func(string)
	enabled bool
}

func SetLogBroadcaster(sendFunc func(string)) {
	logBroadcaster.mu.Lock()
	defer logBroadcaster.mu.Unlock()
	logBroadcaster.send = sendFunc
	logBroadcaster.enabled = sendFunc != nil
}

func NewPrintLogger(debug bool) {
	PrintLog = &PrintLogger{
		debug: debug,
	}
}

func logMessage(indent string, symbol string, color string, format string, a ...interface{}) {
	timestamp := tformat.SetBold(time.Now().Format("02/01 15:04:05"))
	message := fmt.Sprintf(format, a...)
	mark := tformat.SetColor(symbol, color)
	logEntry := fmt.Sprintf("[msf] %s%s %s", indent, mark, message)

	// 输出到控制台
	fmt.Printf("%s %s [%s]\n", timestamp, mark, message)

	// 广播到 WebSocket
	logBroadcaster.mu.RLock()
	if logBroadcaster.enabled && logBroadcaster.send != nil {
		logBroadcaster.send(logEntry)
	}
	logBroadcaster.mu.RUnlock()
}

func Info(indent string, format string, a ...interface{}) {
	logMessage(indent, "[*]", tformat.Green, format, a...)
}

func Success(indent string, format string, a ...interface{}) {
	logMessage(indent, "[+]", tformat.Blue, format, a...)
}

func Warn(indent string, format string, a ...interface{}) {
	logMessage(indent, "[!]", tformat.Yellow, format, a...)
}

func Error(indent string, format string, a ...interface{}) {
	logMessage(indent, "[-]", tformat.Red, format, a...)
}

func Debug(indent string, format string, a ...interface{}) {
	if kServerDebugLogs && PrintLog.debug {
		logMessage(indent, "[#]", tformat.Cyan, format, a...)
	}
}
