#include <Workers/MCP/MCPCommandHandler.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>

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
    if (request.type == LIST_TARGETS)         return handleListTargets(request);
    if (request.type == LIST_PIVOTS)          return handleListPivots(request);
    if (request.type == LIST_COLLECTED_DATA)  return handleListCollectedData(request);
    if (request.type == GET_UI_INFO)          return handleGetUIInfo(request);
    if (request.type == GET_CAPABILITIES)     return handleGetCapabilities(request);
    
    return MCP::MCPResponse::notSupported(request.requestId, request.type);
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
    // TODO: Implement listener management via server API
    return MCP::MCPResponse::error(req.requestId, "Not implemented yet");
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
    // TODO: Implement tunnel management via server API
    return MCP::MCPResponse::error(req.requestId, "Not implemented yet");
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

MCP::MCPResponse MCPCommandHandler::handleGetUIInfo(const MCP::MCPRequest& req)
{
    Q_UNUSED(req)
    
    QJsonObject data;
    
    // Window info
    QWidget* mainWindow = adaptixWidget->window();
    if (mainWindow) {
        QJsonObject windowInfo;
        windowInfo["title"] = mainWindow->windowTitle();
        windowInfo["width"] = mainWindow->width();
        windowInfo["height"] = mainWindow->height();
        windowInfo["x"] = mainWindow->x();
        windowInfo["y"] = mainWindow->y();
        data["window"] = windowInfo;
    }
    
    // Agents count
    data["agents_count"] = adaptixWidget->AgentsMap.size();
    data["listeners_count"] = adaptixWidget->Listeners.size();
    data["tasks_count"] = adaptixWidget->TasksMap.size();
    data["targets_count"] = adaptixWidget->Targets.size();
    data["credentials_count"] = adaptixWidget->Credentials.size();
    data["tunnels_count"] = adaptixWidget->Tunnels.size();
    data["downloads_count"] = adaptixWidget->Downloads.size();
    data["screenshots_count"] = adaptixWidget->Screenshots.size();
    
    // Registered agents and listeners
    QJsonArray regAgents;
    for (const auto& agent : adaptixWidget->RegisterAgents) {
        QJsonObject obj;
        obj["name"] = agent.name;
        obj["listener_type"] = agent.listenerType;
        obj["os"] = agent.os;
        regAgents.append(obj);
    }
    data["registered_agents"] = regAgents;
    
    QJsonArray regListeners;
    for (const auto& listener : adaptixWidget->RegisterListeners) {
        QJsonObject obj;
        obj["name"] = listener.name;
        obj["protocol"] = listener.protocol;
        obj["type"] = listener.type;
        regListeners.append(obj);
    }
    data["registered_listeners"] = regListeners;
    
    return MCP::MCPResponse::success(req.requestId, "", data);
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
    addCap("list_tasks", "List tasks");
    addCap("get_task_output", "Get task output");
    addCap("list_listeners", "List listeners");
    addCap("list_tunnels", "List tunnels");
    addCap("list_targets", "List discovered targets");
    addCap("list_pivots", "List pivots");
    addCap("list_collected_data", "List credentials/downloads/screenshots");
    addCap("update_agent_config", "Update agent sleep/jitter");
    addCap("update_agent_metadata", "Update agent tag/mark");
    addCap("get_ui_info", "Get UI and data summary info");
    
    QJsonObject data;
    data["capabilities"] = capabilities;
    data["total"] = capabilities.size();
    return MCP::MCPResponse::success(req.requestId, "", data);
}
