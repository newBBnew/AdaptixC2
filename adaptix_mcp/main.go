package main

import (
	"flag"
	"fmt"
	"os"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/client"
	"github.com/adaptix/adaptix_mcp/pkg/mcp"
	"github.com/adaptix/adaptix_mcp/pkg/utils"
)

func main() {
	// DEBUG: Verify if IDE actually starts the process
	debugFile, _ := os.OpenFile("/tmp/adaptix_debug.log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0666)
	if debugFile != nil {
		debugFile.WriteString("----------------------------------------\n")
		debugFile.WriteString("Process started at " + time.Now().String() + "\n")
		debugFile.WriteString("Args: " + fmt.Sprintf("%v", os.Args) + "\n")
		debugFile.WriteString("UID: " + fmt.Sprintf("%d", os.Getuid()) + "\n")
		debugFile.Close()
	}

	clientURL := flag.String("url", "ws://127.0.0.1:9999", "Client MCP Bridge URL")
	flag.Parse()

	utils.InfoLogger.Println("🚀 Starting AdaptixC2 MCP Server...")
	utils.InfoLogger.Printf("📡 Client URL: %s", *clientURL)

	connector := client.NewConnector(*clientURL, nil) // Will be set by MCPServer

	server := mcp.NewMCPServer(connector)

	if err := server.Start(); err != nil {
		utils.ErrorLogger.Printf("❌ Failed to start MCP Server: %v", err)
		os.Exit(1)
	}
}
