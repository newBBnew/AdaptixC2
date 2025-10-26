#ifndef ADAPTIX_MCP_AGENTHANDLER_H
#define ADAPTIX_MCP_AGENTHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

// Forward declarations
class AdaptixWidget;

/**
 * @brief Agent操作Handler
 * 
 * 提供Agent的操作功能
 * 支持命令:
 * - remove: 删除Agent
 * - update_config: 更新Agent配置(sleep, jitter等)
 * - set_tag: 设置Agent标签
 * - set_mark: 设置Agent标记
 */
class AgentHandler : public IMCPCommandHandler {
public:
    explicit AgentHandler(AdaptixWidget* widget);
    ~AgentHandler() override = default;
    
    // IMCPCommandHandler interface
    QString getCommandType() const override { return "agent"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Manage agents (remove, update config, set tags)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    // 命令处理函数
    MCP::MCPResponse handleRemoveAgent(const MCP::MCPRequest& request);
    MCP::MCPResponse handleUpdateConfig(const MCP::MCPRequest& request);
    MCP::MCPResponse handleSetTag(const MCP::MCPRequest& request);
    MCP::MCPResponse handleSetMark(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_AGENTHANDLER_H

