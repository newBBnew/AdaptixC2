package utils

import (
	"fmt"
	"log"
	"os"
)

var (
	DebugLogger *log.Logger
	InfoLogger  *log.Logger
	WarnLogger  *log.Logger
	ErrorLogger *log.Logger
)

func init() {
	DebugLogger = log.New(os.Stderr, "[DEBUG] ", log.Ltime)
	InfoLogger = log.New(os.Stderr, "[INFO]  ", log.Ltime)
	WarnLogger = log.New(os.Stderr, "[WARN]  ", log.Ltime)
	ErrorLogger = log.New(os.Stderr, "[ERROR] ", log.Ltime)
}

func ToString(v interface{}) string {
	if v == nil {
		return ""
	}
	return fmt.Sprintf("%v", v)
}
