#include <Workers/MCP/MCPCommandHandler.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>
#include <Client/Requestor.h>
#include <Workers/MCP/MCPBridgeWorker.h>

MCPCommandHandler::MCPCommandHandler(AdaptixWidget* widget, QObject* parent)
    : QObject(parent)
    , adaptixWidget(widget)
{
}

MCPCommandHandler::~MCPCommandHandler() = default;

MCP::MCPResponse MCPCommandHandler::handleCommand(const MCP::MCPRequest& request)
{
    using namespace MCP::Commands;
    
    if (request.type == LIST_AGENTS)          return handleListAgents(request);
    if (request.type == GET_AGENT_INFO)       return handleGetAgentInfo(request);
    if (request.type == UPDATE_AGENT_CONFIG)  return handleUpdateAgentConfig(request);
    if (request.type == UPDATE_AGENT_METADATA)return handleUpdateAgentMetadata(request);
    if (request.type == MANAGE_PTY)           return handleManagePty(request);
    if (request.type == EXECUTE_COMMAND)      return handleExecuteCommand(request);
    if (request.type == GET_CONSOLE_OUTPUT)   return handleGetConsoleOutput(request);
    if (request.type == CLEAR_CONSOLE)        return handleClearConsole(request);
    if (request.type == LIST_TASKS)           return handleListTasks(request);
    if (request.type == GET_TASK_OUTPUT)      return handleGetTaskOutput(request);
    if (request.type == DELETE_TASKS)         return handleDeleteTasks(request);
    if (request.type == LIST_LISTENERS)       return handleListListeners(request);
    if (request.type == MANAGE_LISTENER)      return handleManageListener(request);
    if (request.type == LIST_TUNNELS)         return handleListTunnels(request);
    if (request.type == MANAGE_TUNNEL)        return handleManageTunnel(request);
    if (request.type == LIST_FILEDELIVERY)    return handleListFileDelivery(request);
    if (request.type == MANAGE_FILEDELIVERY)  return handleManageFileDelivery(request);
    if (request.type == LIST_TARGETS)         return handleListTargets(request);
    if (request.type == MANAGE_TARGET)        return handleManageTarget(request);
    if (request.type == LIST_PIVOTS)          return handleListPivots(request);
    if (request.type == LIST_COLLECTED_DATA)  return handleListCollectedData(request);
    if (request.type == GET_CAPABILITIES)     return handleGetCapabilities(request);
    
    // Consolidated Commands (Scientific MCP)
    if (request.type == LOOK_ASSETS)          return handleLookAssets(request);
    if (request.type == LISTEN_INTELLIGENCE)  return handleListenIntelligence(request);
    if (request.type == SPEAK_INTERACTION)   return handleSpeakInteraction(request);
    if (request.type == WRITE_ORCHESTRATION)  return handleWriteOrchestration(request);
    if (request.type == OPERATE_CONTROL)      return handleOperateControl(request);

    // Tactical Workflow
    if (request.type == TACTICAL_GET_LIBRARY)      return handleTacticalGetLibrary(request);
    if (request.type == TACTICAL_MODIFY_WORKFLOW) return handleTacticalModifyWorkflow(request);
    if (request.type == TACTICAL_EXECUTE_SEQUENCE) return handleTacticalExecuteSequence(request);
    if (request.type == TACTICAL_READ_RESULTS)     return handleTacticalReadResults(request);
    if (request.type == TACTICAL_MODIFY_LIBRARY)   return handleTacticalModifyLibrary(request);
    if (request.type == TACTICAL_BROADCAST_SUGGESTION) return handleTacticalBroadcastSuggestion(request);
    if (request.type == SEND_TEAM_CHAT)            return handleSendTeamChat(request);
    if (request.type == TACTICAL_CHAT_RESPONSE)    return handleTacticalChatResponse(request);
    
    // Session Management
    if (request.type == ARCHIVE_SESSION)           return handleArchiveSession(request);
    if (request.type == LIST_SESSIONS)             return handleListSessions(request);
    if (request.type == GET_SESSION_CONTENT)       return handleReadSession(request);

    if (request.type == GOD_VIEW_QUERY_STATUS)    return handleGodViewQueryStatus(request);
    if (request.type == GOD_VIEW_SUGGEST_ACTION)  return handleGodViewSuggestAction(request);
    if (request.type == AI_AUTONOMOUS_CONTROL)     return handleAiAutonomousControl(request);
    
    return MCP::MCPResponse::notSupported(request.requestId, request.type);
}

#include <UI/Widgets/TacticalGuidanceWidget.h>

MCP::MCPResponse MCPCommandHandler::handleLookAssets(const MCP::MCPRequest& req)
{
    QString type = req.params["type"].toString();
    if (type == "agents")    return handleListAgents(req);
    if (type == "listeners") return handleListListeners(req);
    if (type == "targets")   return handleListTargets(req);
    if (type == "tunnels")   return handleListTunnels(req);
    if (type == "pivots")    return handleListPivots(req);
    
    return MCP::MCPResponse::error(req.requestId, "Unknown asset type: " + type);
}

MCP::MCPResponse MCPCommandHandler::handleListenIntelligence(const MCP::MCPRequest& req)
{
    QString type = req.params["type"].toString();
    if (type == "console")        return handleGetConsoleOutput(req);
    if (type == "tasks")          return handleListTasks(req);
    if (type == "task_output")    return handleGetTaskOutput(req);
    if (type == "collected_data") return handleListCollectedData(req);
    
    return MCP::MCPResponse::error(req.requestId, "Unknown intelligence type: " + type);
}

MCP::MCPResponse MCPCommandHandler::handleSpeakInteraction(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    if (action == "broadcast") return handleTacticalBroadcastSuggestion(req);
    if (action == "enter_chat") {
        // God View: AI 通知团队它已进入聊天模式
        HttpReqChatSendMessageAsync("[AI] I am now active and monitoring team chat.", *adaptixWidget->GetProfile(), [](bool, const QString&, const QJsonObject&){});
        return MCP::MCPResponse::success(req.requestId, "AI entered team chat mode");
    }
    
    return MCP::MCPResponse::error(req.requestId, "Unknown interaction action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleWriteOrchestration(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    if (action == "modify_workflow")     return handleTacticalModifyWorkflow(req);
    if (action == "modify_library")      return handleTacticalModifyLibrary(req);
    if (action == "update_agent_config") return handleUpdateAgentConfig(req);
    if (action == "update_agent_metadata") return handleUpdateAgentMetadata(req);
    
    return MCP::MCPResponse::error(req.requestId, "Unknown orchestration action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleOperateControl(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    if (action == "execute")  return handleExecuteCommand(req);
    if (action == "tunnel")   return handleManageTunnel(req);
    if (action == "file")     return handleManageFileDelivery(req);
    if (action == "pty")      return handleManagePty(req);
    if (action == "listener") return handleManageListener(req);
    
    return MCP::MCPResponse::error(req.requestId, "Unknown control action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleTacticalGetLibrary(const MCP::MCPRequest& req)
{
    if (!adaptixWidget->TacticalGuidanceDock)
        return MCP::MCPResponse::error(req.requestId, "Tactical module not found");

    QJsonObject data = adaptixWidget->TacticalGuidanceDock->getLibraryAsJson();
    return MCP::MCPResponse::success(req.requestId, "Catalog retrieved", data);
}

MCP::MCPResponse MCPCommandHandler::handleTacticalModifyWorkflow(const MCP::MCPRequest& req)
{
    if (!adaptixWidget->TacticalGuidanceDock)
        return MCP::MCPResponse::error(req.requestId, "Tactical module not found");

    QString action = req.params["action"].toString();
    if (action == "add_step") {
        QString variantId = req.params["variant_id"].toString();
        QJsonObject paramsObj = req.params["parameters"].toObject();
        QMap<QString, QString> params;
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            params[it.key()] = it.value().toString();
        }
        adaptixWidget->TacticalGuidanceDock->addStepToActivePlaybook(variantId, params);
        return MCP::MCPResponse::success(req.requestId, "Step added to workflow");
    } else if (action == "clear") {
        adaptixWidget->TacticalGuidanceDock->clearWorkflow();
        return MCP::MCPResponse::success(req.requestId, "Workflow cleared");
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported tactical action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleTacticalExecuteSequence(const MCP::MCPRequest& req)
{
    if (!adaptixWidget->TacticalGuidanceDock)
        return MCP::MCPResponse::error(req.requestId, "Tactical module not found");

    adaptixWidget->TacticalGuidanceDock->runActivePlaybook();
    return MCP::MCPResponse::success(req.requestId, "Tactical execution triggered");
}

MCP::MCPResponse MCPCommandHandler::handleTacticalReadResults(const MCP::MCPRequest& req)
{
    if (!adaptixWidget->TacticalGuidanceDock)
        return MCP::MCPResponse::error(req.requestId, "Tactical module not found");

    QJsonObject data = adaptixWidget->TacticalGuidanceDock->getResultsAsJson();
    return MCP::MCPResponse::success(req.requestId, "Results retrieved", data);
}

MCP::MCPResponse MCPCommandHandler::handleTacticalModifyLibrary(const MCP::MCPRequest& req)
{
    if (!adaptixWidget->TacticalGuidanceDock)
        return MCP::MCPResponse::error(req.requestId, "Tactical module not found");

    QString action = req.params["action"].toString();
    // Library modification is now local-only and handled via UI context menus or local API if implemented.
    // Server sync logic has been removed.
    
    return MCP::MCPResponse::error(req.requestId, "Library modification via MCP is currently disabled (Local Mode only).");
}

MCP::MCPResponse MCPCommandHandler::handleTacticalBroadcastSuggestion(const MCP::MCPRequest& req)
{
    QString content = req.params["content"].toString();
    if (content.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing content parameter");

    HttpReqTacticalSuggestionSendAsync(content, *adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject&) {
        if (success)
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Suggestion broadcasted"));
        else
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
    });
    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleSendTeamChat(const MCP::MCPRequest& req)
{
    QString content = req.params["content"].toString();
    if (content.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing content parameter");

    // LogInfo("[MCP] AI sending team chat: %s", content.toUtf8().constData());

    HttpReqChatSendMessageAsync(content, *adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject&) {
        if (success) {
            // LogInfo("[MCP] AI team chat message sent successfully to server");
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Message sent to team chat"));
        } else {
            LogError("[MCP] Failed to send AI team chat: %s", message.toUtf8().constData());
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        }
    });
    return MCP::MCPResponse::deferred();
}

QJsonObject MCPCommandHandler::agentToJson(const QString& agentId)
{
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return QJsonObject();
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    QJsonObject obj;
    obj["id"] = agent->data.Id;
    obj["name"] = agent->data.Name;
    obj["listener"] = agent->data.Listener;
    obj["external_ip"] = agent->data.ExternalIP;
    obj["internal_ip"] = agent->data.InternalIP;
    obj["computer"] = agent->data.Computer;
    obj["username"] = agent->data.Username;
    obj["domain"] = agent->data.Domain;
    obj["os"] = agent->data.Os;
    obj["os_desc"] = agent->data.OsDesc;
    obj["arch"] = agent->data.Arch;
    obj["process"] = agent->data.Process;
    obj["pid"] = agent->data.Pid;
    obj["elevated"] = agent->data.Elevated;
    obj["sleep"] = agent->data.Sleep;
    obj["jitter"] = agent->data.Jitter;
    obj["last_tick"] = agent->data.LastTick;
    obj["tags"] = agent->data.Tags;
    obj["mark"] = agent->data.Mark;
    obj["date"] = agent->data.Date;
    return obj;
}

QJsonObject MCPCommandHandler::taskToJson(const TaskData& task)
{
    QJsonObject obj;
    obj["task_id"] = task.TaskId;
    obj["agent_id"] = task.AgentId;
    obj["task_type"] = task.TaskType;
    obj["command_line"] = task.CommandLine;
    obj["status"] = task.Status;
    obj["message"] = task.Message;
    obj["output"] = task.Output;
    obj["completed"] = task.Completed;
    obj["start_time"] = task.StartTime;
    obj["finish_time"] = task.FinishTime;
    return obj;
}

QJsonObject MCPCommandHandler::listenerToJson(const ListenerData& listener)
{
    QJsonObject obj;
    obj["name"] = listener.Name;
    obj["type"] = listener.ListenerType;
    obj["protocol"] = listener.ListenerProtocol;
    obj["bind_host"] = listener.BindHost;
    obj["bind_port"] = listener.BindPort;
    obj["status"] = listener.Status;
    obj["date"] = listener.Date;
    return obj;
}

QJsonObject MCPCommandHandler::tunnelToJson(const TunnelData& tunnel)
{
    QJsonObject obj;
    obj["tunnel_id"] = tunnel.TunnelId;
    obj["agent_id"] = tunnel.AgentId;
    obj["type"] = tunnel.Type;
    obj["interface"] = tunnel.Interface;
    obj["port"] = tunnel.Port;
    obj["info"] = tunnel.Info;
    return obj;
}

QJsonObject MCPCommandHandler::targetToJson(const TargetData& target)
{
    QJsonObject obj;
    obj["target_id"] = target.TargetId;
    obj["computer"] = target.Computer;
    obj["domain"] = target.Domain;
    obj["address"] = target.Address;
    obj["os"] = target.Os;
    obj["os_desc"] = target.OsDesc;
    obj["alive"] = target.Alive;
    obj["tag"] = target.Tag;
    return obj;
}

QJsonObject MCPCommandHandler::credentialToJson(const CredentialData& cred)
{
    QJsonObject obj;
    obj["cred_id"] = cred.CredId;
    obj["username"] = cred.Username;
    obj["password"] = cred.Password;
    obj["realm"] = cred.Realm;
    obj["type"] = cred.Type;
    obj["tag"] = cred.Tag;
    obj["storage"] = cred.Storage;
    obj["host"] = cred.Host;
    return obj;
}

MCP::MCPResponse MCPCommandHandler::handleListAgents(const MCP::MCPRequest& req)
{
    QJsonArray agents;
    for (auto it = adaptixWidget->AgentsMap.begin(); it != adaptixWidget->AgentsMap.end(); ++it) {
        agents.append(agentToJson(it.key()));
    }
    
    QJsonObject data;
    data["agents"] = agents;
    data["total"] = agents.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleGetAgentInfo(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    QJsonObject data;
    data["agent"] = agentToJson(agentId);
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleUpdateAgentConfig(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    
    int sleep = req.params["sleep"].toInt(-1);
    int jitter = req.params["jitter"].toInt(-1);
    
    if (sleep >= 0 || jitter >= 0) {
        QString cmd = QString("sleep %1 %2").arg(sleep >= 0 ? sleep : agent->data.Sleep)
                                            .arg(jitter >= 0 ? jitter : agent->data.Jitter);
        agent->Console->SetInput(cmd);
        agent->Console->processInput();
    }
    
    QJsonObject data;
    data["agent"] = agentToJson(agentId);
    return MCP::MCPResponse::success(req.requestId, "Config updated", data);
}

MCP::MCPResponse MCPCommandHandler::handleUpdateAgentMetadata(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    QString metaType = req.params["metadata_type"].toString();
    QString value = req.params["value"].toString();
    
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    
    if (metaType == "tag") {
        agent->data.Tags = value;
    } else if (metaType == "mark") {
        agent->data.Mark = value;
    }
    
    QJsonObject data;
    data["agent"] = agentToJson(agentId);
    return MCP::MCPResponse::success(req.requestId, "Metadata updated", data);
}

#include <Workers/MCP/MCPTerminalManager.h>

MCP::MCPResponse MCPCommandHandler::handleManagePty(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    if (action == "open") {
        QString agentId = req.params["agent_id"].toString();
        QString program = req.params["program"].toString();
        int rows = req.params["rows"].toInt(24);
        int cols = req.params["cols"].toInt(80);

        if (agentId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");

        if (!adaptixWidget->AgentsMap.contains(agentId))
            return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);

        Agent* agent = adaptixWidget->AgentsMap[agentId];
        auto profile = adaptixWidget->GetProfile();

        QString urlTemplate = "wss://%1:%2%3/channel";
        QString sUrl = urlTemplate.arg(profile->GetHost()).arg(profile->GetPort()).arg(profile->GetEndpoint());
        QString token = profile->GetAccessToken();
        int oemCP = agent->data.OemCP;

        if (program.isEmpty()) {
            if (agent->data.Os == OS_WINDOWS) program = "C:\\Windows\\System32\\cmd.exe";
            else if (agent->data.Os == OS_LINUX) program = "/bin/sh";
            else program = "/bin/zsh";
        }

        QString ptyId = MCPTerminalManager::instance()->openSession(agentId, program, rows, cols, token, QUrl(sUrl), oemCP);

        QJsonObject data;
        data["pty_id"] = ptyId;
        data["agent_id"] = agentId;
        data["program"] = program;
        return MCP::MCPResponse::success(req.requestId, "PTY session opened", data);
    }
    else if (action == "write") {
        QString ptyId = req.params["pty_id"].toString();
        QString text = req.params["data"].toString();
        bool base64 = req.params["base64"].toBool(false);

        if (ptyId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing pty_id parameter");

        QByteArray data;
        if (base64) {
            data = QByteArray::fromBase64(text.toUtf8());
        } else {
            data = text.toUtf8();
        }

        if (MCPTerminalManager::instance()->writeSession(ptyId, data)) {
            return MCP::MCPResponse::success(req.requestId, "Data sent to PTY", QJsonObject());
        } else {
            return MCP::MCPResponse::error(req.requestId, "PTY session not found: " + ptyId);
        }
    }
    else if (action == "read") {
        QString ptyId = req.params["pty_id"].toString();
        bool clear = req.params["clear"].toBool(true);

        if (ptyId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing pty_id parameter");

        QByteArray output = MCPTerminalManager::instance()->readSession(ptyId, clear);
        
        QJsonObject data;
        data["pty_id"] = ptyId;
        data["data"] = QString::fromUtf8(output.toBase64());
        data["text"] = MCPTerminalManager::instance()->cleanAnsi(output);
        data["length"] = output.size();
        data["format"] = "base64";
        
        return MCP::MCPResponse::success(req.requestId, "", data);
    }
    else if (action == "close") {
        QString ptyId = req.params["pty_id"].toString();

        if (ptyId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing pty_id parameter");

        MCPTerminalManager::instance()->closeSession(ptyId);
        return MCP::MCPResponse::success(req.requestId, "PTY session closed", QJsonObject());
    }
    else if (action == "list") {
        QJsonArray ptyList;
        for (const QString& id : MCPTerminalManager::instance()->getSessionIds()) {
            ptyList.append(id);
        }
        
        QJsonObject data;
        data["ptys"] = ptyList;
        data["total"] = ptyList.size();
        return MCP::MCPResponse::success(req.requestId, "", data);
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported PTY action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleExecuteCommand(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    QString command = req.params["command"].toString();
    
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    if (command.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing command parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    agent->Console->SetInput(command);
    agent->Console->processInput();
    
    QJsonObject data;
    data["agent_id"] = agentId;
    data["command"] = command;
    data["status"] = "submitted";
    return MCP::MCPResponse::success(req.requestId, "Command submitted", data);
}

MCP::MCPResponse MCPCommandHandler::handleGetConsoleOutput(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    QString output = agent->Console->GetOutput();
    
    QJsonObject data;
    data["agent_id"] = agentId;
    data["output"] = output;
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleClearConsole(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    if (agentId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing agent_id parameter");
    
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return MCP::MCPResponse::error(req.requestId, "Agent not found: " + agentId);
    
    Agent* agent = adaptixWidget->AgentsMap[agentId];
    agent->Console->Clear();
    
    return MCP::MCPResponse::success(req.requestId, "Console cleared");
}

MCP::MCPResponse MCPCommandHandler::handleListTasks(const MCP::MCPRequest& req)
{
    QString agentId = req.params["agent_id"].toString();
    
    QJsonArray tasks;
    for (auto it = adaptixWidget->TasksMap.begin(); it != adaptixWidget->TasksMap.end(); ++it) {
        if (agentId.isEmpty() || it->AgentId == agentId) {
            tasks.append(taskToJson(*it));
        }
    }
    
    QJsonObject data;
    data["tasks"] = tasks;
    data["total"] = tasks.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleGetTaskOutput(const MCP::MCPRequest& req)
{
    QString taskId = req.params["task_id"].toString();
    if (taskId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing task_id parameter");
    
    if (!adaptixWidget->TasksMap.contains(taskId))
        return MCP::MCPResponse::error(req.requestId, "Task not found: " + taskId);
    
    QJsonObject data;
    data["task"] = taskToJson(adaptixWidget->TasksMap[taskId]);
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleDeleteTasks(const MCP::MCPRequest& req)
{
    // TODO: Implement task deletion via server API
    return MCP::MCPResponse::error(req.requestId, "Not implemented yet");
}

MCP::MCPResponse MCPCommandHandler::handleListListeners(const MCP::MCPRequest& req)
{
    Q_UNUSED(req)
    QJsonArray listeners;
    for (const auto& listener : adaptixWidget->Listeners) {
        listeners.append(listenerToJson(listener));
    }
    
    QJsonObject data;
    data["listeners"] = listeners;
    data["total"] = listeners.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleManageListener(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    QString name = req.params["name"].toString();
    QString type = req.params["type"].toString();

    if (action.isEmpty() || name.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing action or name parameter");

    if (action == "start") {
        QString configData = req.params["data"].toString();
        if (type.isEmpty() || configData.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing type or data for start action");

        HttpReqListenerStartAsync(name, type, configData, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Listener started", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "stop") {
        if (type.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing type for stop action");

        HttpReqListenerStopAsync(name, type, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Listener stopped", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "edit") {
        QString configData = req.params["data"].toString();
        if (type.isEmpty() || configData.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing type or data for edit action");

        HttpReqListenerEditAsync(name, type, configData, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Listener updated", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleListTunnels(const MCP::MCPRequest& req)
{
    Q_UNUSED(req)
    QJsonArray tunnels;
    for (const auto& tunnel : adaptixWidget->Tunnels) {
        tunnels.append(tunnelToJson(tunnel));
    }
    
    QJsonObject data;
    data["tunnels"] = tunnels;
    data["total"] = tunnels.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleManageTunnel(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    
    if (action.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing action parameter");

    if (action == "start") {
        QString tunnelType = req.params["type"].toString();
        QJsonObject tunnelData;
        
        if (req.params["data"].isObject()) {
            tunnelData = req.params["data"].toObject();
        } else if (req.params["data"].isString()) {
            QString dataStr = req.params["data"].toString();
            QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8());
            if (doc.isObject()) {
                tunnelData = doc.object();
            }
        }
        
        if (tunnelType.isEmpty() || tunnelData.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing type or data for start action (data must be an object or a valid JSON string)");

        QByteArray jsonData = QJsonDocument(tunnelData).toJson();
        HttpReqTunnelStartServerAsync(tunnelType, jsonData, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Tunnel started", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "stop") {
        QString tunnelId = req.params["tunnel_id"].toString();
        if (tunnelId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing tunnel_id for stop action");

        HttpReqTunnelStopAsync(tunnelId, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Tunnel stopped", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "edit") {
        QString tunnelId = req.params["tunnel_id"].toString();
        QString info = req.params["info"].toString();
        if (tunnelId.isEmpty() || info.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing tunnel_id or info for edit action");

        HttpReqTunnelSetInfoAsync(tunnelId, info, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Tunnel info updated", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleListFileDelivery(const MCP::MCPRequest& req)
{
    HttpReqFileDeliveryListAsync(*(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject& data) {
        if (success)
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "", data));
        else
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
    });
    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleManageFileDelivery(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    if (action.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing action parameter");

    if (action == "upload") {
        QString filePath = req.params["local_path"].toString();
        QString fileName = req.params["file_name"].toString();
        
        if (filePath.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing local_path for upload");
            
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return MCP::MCPResponse::error(req.requestId, "Failed to open local file: " + file.errorString());
            
        QByteArray fileData = file.readAll();
        file.close();
        
        if (fileName.isEmpty())
            fileName = QFileInfo(filePath).fileName();

        HttpReqFileDeliveryUploadAsync(fileName, fileData, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject& data) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "File uploaded", data));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "delete") {
        QString fileId = req.params["file_id"].toString();
        if (fileId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing file_id for delete action");

        HttpReqFileDeliveryDeleteAsync(fileId, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "File deleted", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }
    else if (action == "create_link") {
        QString fileId = req.params["file_id"].toString();
        int expireHours = req.params["expire_hours"].toInt(24);
        int maxUses = req.params["max_uses"].toInt(0);
        QString allowedIp = req.params["allowed_ip"].toString();

        if (fileId.isEmpty())
            return MCP::MCPResponse::error(req.requestId, "Missing file_id for link creation");

        HttpReqFileDeliveryLinkCreateAsync(fileId, expireHours, maxUses, allowedIp, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject& data) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Link created", data));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleListTargets(const MCP::MCPRequest& req)
{
    Q_UNUSED(req)
    QJsonArray targets;
    for (const auto& target : adaptixWidget->Targets) {
        targets.append(targetToJson(target));
    }
    
    QJsonObject data;
    data["targets"] = targets;
    data["total"] = targets.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleManageTarget(const MCP::MCPRequest& req)
{
    QString action = req.params["action"].toString();
    QStringList targetIds = req.params["target_ids"].toVariant().toStringList();

    if (targetIds.isEmpty()) {
        QString singleId = req.params["target_id"].toString();
        if (!singleId.isEmpty())
            targetIds.append(singleId);
    }

    if (action.isEmpty() || targetIds.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing action or target_ids parameter");

    if (action == "remove") {
        HttpReqTargetRemoveAsync(targetIds, *(adaptixWidget->GetProfile()), [this, req](bool success, const QString& message, const QJsonObject&) {
            if (success)
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Targets removed", QJsonObject()));
            else
                adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        });
        return MCP::MCPResponse::deferred();
    }

    return MCP::MCPResponse::error(req.requestId, "Unsupported action: " + action);
}

MCP::MCPResponse MCPCommandHandler::handleListPivots(const MCP::MCPRequest& req)
{
    Q_UNUSED(req)
    QJsonArray pivots;
    for (auto it = adaptixWidget->Pivots.begin(); it != adaptixWidget->Pivots.end(); ++it) {
        QJsonObject obj;
        obj["pivot_id"] = it->PivotId;
        obj["pivot_name"] = it->PivotName;
        obj["parent_agent_id"] = it->ParentAgentId;
        obj["child_agent_id"] = it->ChildAgentId;
        pivots.append(obj);
    }
    
    QJsonObject data;
    data["pivots"] = pivots;
    data["total"] = pivots.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleListCollectedData(const MCP::MCPRequest& req)
{
    QString dataType = req.params["data_type"].toString();
    QJsonObject data;
    
    if (dataType == "credentials") {
        QJsonArray creds;
        for (const auto& cred : adaptixWidget->Credentials) {
            creds.append(credentialToJson(cred));
        }
        data["credentials"] = creds;
        data["total"] = creds.size();
    }
    else if (dataType == "downloads") {
        QJsonArray downloads;
        for (auto it = adaptixWidget->Downloads.begin(); it != adaptixWidget->Downloads.end(); ++it) {
            QJsonObject obj;
            obj["file_id"] = it->FileId;
            obj["agent_id"] = it->AgentId;
            obj["filename"] = it->Filename;
            obj["total_size"] = it->TotalSize;
            obj["recv_size"] = it->RecvSize;
            obj["state"] = it->State;
            downloads.append(obj);
        }
        data["downloads"] = downloads;
        data["total"] = downloads.size();
    }
    else if (dataType == "screenshots") {
        QJsonArray screens;
        for (auto it = adaptixWidget->Screenshots.begin(); it != adaptixWidget->Screenshots.end(); ++it) {
            QJsonObject obj;
            obj["screen_id"] = it->ScreenId;
            obj["user"] = it->User;
            obj["computer"] = it->Computer;
            obj["note"] = it->Note;
            obj["date"] = it->Date;
            screens.append(obj);
        }
        data["screenshots"] = screens;
        data["total"] = screens.size();
    }
    else {
        return MCP::MCPResponse::error(req.requestId, "Invalid data_type. Use: credentials, downloads, screenshots");
    }
    
    return MCP::MCPResponse::success(req.requestId, "", data);
}

MCP::MCPResponse MCPCommandHandler::handleTacticalChatResponse(const MCP::MCPRequest& req)
{
    QString content = req.params["content"].toString();
    if (content.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing content parameter");

    LogInfo("[MCP] AI replying to tactical/team chat: %s", content.toUtf8().constData());

    // God View: 所有的回复现在都直接通过团队聊天发送
    HttpReqChatSendMessageAsync("[AI] " + content, *adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject&) {
        if (success) {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "AI reply sent to team chat"));
        } else {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, "Failed to send AI reply: " + message));
        }
    });

    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleArchiveSession(const MCP::MCPRequest& req)
{
    HttpReqSessionArchiveAsync(*adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject& data) {
        if (success) {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Session archived", data));
        } else {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        }
    });
    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleListSessions(const MCP::MCPRequest& req)
{
    HttpReqSessionListAsync(*adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject& data) {
        if (success) {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "", data));
        } else {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        }
    });
    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleReadSession(const MCP::MCPRequest& req)
{
    QString sessionId = req.params["session_id"].toString();
    if (sessionId.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing session_id parameter");

    HttpReqSessionContentAsync(sessionId, *adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject& data) {
        if (success) {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "", data));
        } else {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, message));
        }
    });
    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleGodViewQueryStatus(const MCP::MCPRequest& req)
{
    QJsonObject fullStatus;
    
    // 聚合所有关键信息
    QJsonArray agents;
    for (auto it = adaptixWidget->AgentsMap.begin(); it != adaptixWidget->AgentsMap.end(); ++it)
        agents.append(agentToJson(it.key()));
    fullStatus["agents"] = agents;

    QJsonArray listeners;
    for (const auto& listener : adaptixWidget->Listeners)
        listeners.append(listenerToJson(listener));
    fullStatus["listeners"] = listeners;

    QJsonArray targets;
    for (const auto& target : adaptixWidget->Targets)
        targets.append(targetToJson(target));
    fullStatus["targets"] = targets;

    QJsonArray tasks;
    for (auto it = adaptixWidget->TasksMap.begin(); it != adaptixWidget->TasksMap.end(); ++it)
        tasks.append(taskToJson(*it));
    fullStatus["tasks"] = tasks;

    return MCP::MCPResponse::success(req.requestId, "Full status retrieved", fullStatus);
}

MCP::MCPResponse MCPCommandHandler::handleGodViewSuggestAction(const MCP::MCPRequest& req)
{
    QString suggestion = req.params["suggestion"].toString();
    QString reasoning = req.params["reasoning"].toString();
    
    if (suggestion.isEmpty())
        return MCP::MCPResponse::error(req.requestId, "Missing suggestion parameter");

    LogInfo("[MCP] AI suggesting action: %s", suggestion.toUtf8().constData());

    QString fullMessage = "[AI Suggestion] " + suggestion;
    if (!reasoning.isEmpty()) {
        fullMessage += "\nReasoning: " + reasoning;
    }

    HttpReqChatSendMessageAsync(fullMessage, *adaptixWidget->GetProfile(), [this, req](bool success, const QString& message, const QJsonObject&) {
        if (success) {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::success(req.requestId, "Suggestion sent to team chat"));
        } else {
            adaptixWidget->McpBridge->sendResponse(MCP::MCPResponse::error(req.requestId, "Failed to send suggestion: " + message));
        }
    });

    return MCP::MCPResponse::deferred();
}

MCP::MCPResponse MCPCommandHandler::handleAiAutonomousControl(const MCP::MCPRequest& req)
{
    bool enabled = req.params["enabled"].toBool(false);
    LogInfo("[MCP] AI Autonomy set to: %s", enabled ? "ENABLED" : "DISABLED");
    
    // 这里可以持久化设置或通知其他组件
    return MCP::MCPResponse::success(req.requestId, QString("AI autonomy %1").arg(enabled ? "enabled" : "disabled"));
}

MCP::MCPResponse MCPCommandHandler::handleGetCapabilities(const MCP::MCPRequest& req)
{
    QJsonArray capabilities;
    
    auto addCap = [&](const QString& name, const QString& desc) {
        QJsonObject cap;
        cap["name"] = name;
        cap["description"] = desc;
        cap["available"] = true;
        capabilities.append(cap);
    };
    
    addCap("list_agents", "List all connected agents");
    addCap("get_agent_info", "Get detailed agent information");
    addCap("execute_command", "Execute command on agent");
    addCap("get_console_output", "Get agent console output");
    addCap("clear_console", "Clear agent console");
    addCap("list_tasks", "List tasks (optionally filtered by agent)");
    addCap("get_task_output", "Get task output");
    addCap("list_listeners", "List listeners");
    addCap("manage_listener", "Manage listeners (start, stop, edit)");
    addCap("list_tunnels", "List tunnels");
    addCap("manage_tunnel", "Manage tunnels (start, stop, edit)");
    addCap("list_targets", "List discovered targets");
    addCap("manage_target", "Manage discovered targets (remove)");
    addCap("list_pivots", "List pivots");
    addCap("list_collected_data", "List credentials/downloads/screenshots");
    addCap("list_filedelivery", "List hosted files for delivery");
    addCap("manage_filedelivery", "Manage hosted files (upload, delete, create link)");
    addCap("update_agent_config", "Update agent sleep/jitter");
    addCap("update_agent_metadata", "Update agent tag/mark");
    addCap("manage_pty", "Manage PTY sessions (open, read, write, close, list)");
    addCap("tactical_chat_response", "Reply to tactical chat with AI suggestions");
    addCap("tactical_execute_sequence", "Execute a sequence of tactical workflow steps");
    addCap("god_view_query_status", "Query full C2 status (Agents, Tasks, Targets, Listeners)");
    addCap("god_view_suggest_action", "Suggest an action to the team via chat");
    addCap("ai_autonomous_control", "Enable/Disable AI autonomous mode");
    
    QJsonObject data;
    data["capabilities"] = capabilities;
    data["total"] = capabilities.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}
