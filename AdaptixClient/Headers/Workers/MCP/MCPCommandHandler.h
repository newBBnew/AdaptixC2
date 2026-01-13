#ifndef MCPCOMMANDHANDLER_H
#define MCPCOMMANDHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include "MCPProtocol.h"
#include <main.h>

class AdaptixWidget;

class MCPCommandHandler : public QObject {
    Q_OBJECT

public:
    explicit MCPCommandHandler(AdaptixWidget* widget, QObject* parent = nullptr);
    ~MCPCommandHandler() override;

    MCP::MCPResponse handleCommand(const MCP::MCPRequest& request);

private:
    MCP::MCPResponse handleListAgents(const MCP::MCPRequest& req);
    MCP::MCPResponse handleGetAgentInfo(const MCP::MCPRequest& req);
    MCP::MCPResponse handleUpdateAgentConfig(const MCP::MCPRequest& req);
    MCP::MCPResponse handleUpdateAgentMetadata(const MCP::MCPRequest& req);
    MCP::MCPResponse handleExecuteCommand(const MCP::MCPRequest& req);
    MCP::MCPResponse handleGetConsoleOutput(const MCP::MCPRequest& req);
    MCP::MCPResponse handleClearConsole(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListTasks(const MCP::MCPRequest& req);
    MCP::MCPResponse handleGetTaskOutput(const MCP::MCPRequest& req);
    MCP::MCPResponse handleDeleteTasks(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListListeners(const MCP::MCPRequest& req);
    MCP::MCPResponse handleManageListener(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListTunnels(const MCP::MCPRequest& req);
    MCP::MCPResponse handleManageTunnel(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListFileDelivery(const MCP::MCPRequest& req);
    MCP::MCPResponse handleManageFileDelivery(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListTargets(const MCP::MCPRequest& req);
    MCP::MCPResponse handleManageTarget(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListPivots(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListCollectedData(const MCP::MCPRequest& req);
    MCP::MCPResponse handleGetCapabilities(const MCP::MCPRequest& req);

    // Consolidated Commands (Scientific MCP)
    MCP::MCPResponse handleLookAssets(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListenIntelligence(const MCP::MCPRequest& req);
    MCP::MCPResponse handleSpeakInteraction(const MCP::MCPRequest& req);
    MCP::MCPResponse handleWriteOrchestration(const MCP::MCPRequest& req);
    MCP::MCPResponse handleOperateControl(const MCP::MCPRequest& req);

    // Tactical Workflow
    MCP::MCPResponse handleTacticalGetLibrary(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalModifyWorkflow(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalExecuteSequence(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalReadResults(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalModifyLibrary(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalBroadcastSuggestion(const MCP::MCPRequest& req);
    MCP::MCPResponse handleSendTeamChat(const MCP::MCPRequest& req);
    MCP::MCPResponse handleTacticalChatResponse(const MCP::MCPRequest& req);
    
    // Session Management
    MCP::MCPResponse handleArchiveSession(const MCP::MCPRequest& req);
    MCP::MCPResponse handleListSessions(const MCP::MCPRequest& req);
    MCP::MCPResponse handleReadSession(const MCP::MCPRequest& req);

    MCP::MCPResponse handleGodViewQueryStatus(const MCP::MCPRequest& req);
    MCP::MCPResponse handleGodViewSuggestAction(const MCP::MCPRequest& req);
    MCP::MCPResponse handleAiAutonomousControl(const MCP::MCPRequest& req);

    // PTY
    MCP::MCPResponse handleManagePty(const MCP::MCPRequest& req);

    QJsonObject agentToJson(const QString& agentId);
    QJsonObject taskToJson(const TaskData& task);
    QJsonObject listenerToJson(const ListenerData& listener);
    QJsonObject tunnelToJson(const TunnelData& tunnel);
    QJsonObject targetToJson(const TargetData& target);
    QJsonObject credentialToJson(const CredentialData& cred);

    AdaptixWidget* adaptixWidget;
};

#endif
