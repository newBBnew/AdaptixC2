#ifndef ADAPTIX_MCP_TUNNELHANDLER_H
#define ADAPTIX_MCP_TUNNELHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

// Forward declarations
class AdaptixWidget;

/**
 * @brief Tunnel管理Handler
 * 
 * 提供Tunnel的查询、创建、停止功能
 * 支持命令:
 * - list: 列出所有Tunnel
 * - get_info: 获取特定Tunnel详情
 * - create: 创建新Tunnel (SOCKS4/5, PortForward)
 * - stop: 停止Tunnel
 */
class TunnelHandler : public IMCPCommandHandler {
public:
    explicit TunnelHandler(AdaptixWidget* widget);
    ~TunnelHandler() override = default;
    
    // IMCPCommandHandler interface
    QString getCommandType() const override { return "tunnel"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Manage tunnels (SOCKS proxy, port forwarding)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    // 命令处理函数
    MCP::MCPResponse handleListTunnels(const MCP::MCPRequest& request);
    MCP::MCPResponse handleGetTunnelInfo(const MCP::MCPRequest& request);
    MCP::MCPResponse handleCreateTunnel(const MCP::MCPRequest& request);
    MCP::MCPResponse handleStopTunnel(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_TUNNELHANDLER_H

