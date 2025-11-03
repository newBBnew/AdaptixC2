#include "AgentHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Agent/Agent.h>
#include <QJsonArray>
#include <QJsonDocument>

using namespace MCP;

AgentHandler::AgentHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse AgentHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "remove") {
        return handleRemoveAgent(request);
    } else if (command == "update_config") {
        return handleUpdateConfig(request);
    } else if (command == "set_tag") {
        return handleSetTag(request);
    } else if (command == "set_mark") {
        return handleSetMark(request);
    } else if (command == "delete_tasks") {
        return handleDeleteTasks(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown agent command: %1").arg(command)
        );
    }
}

MCPResponse AgentHandler::handleRemoveAgent(const MCPRequest& request)
{
    // Support both single agent_id and agent_ids array
    QStringList agentIds;
    
    if (request.params.contains("agent_id")) {
        QString agentId = request.params.value("agent_id").toString();
        if (agentId.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_id"
            );
        }
        agentIds.append(agentId);
    } else if (request.params.contains("agent_ids")) {
        QJsonArray idsArray = request.params.value("agent_ids").toArray();
        if (idsArray.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_ids array"
            );
        }
        for (const QJsonValue& val : idsArray) {
            agentIds.append(val.toString());
        }
    } else {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: agent_id or agent_ids"
        );
    }
    
    // Verify agents exist
    for (const QString& agentId : agentIds) {
        if (!adaptixWidget->AgentsMap.contains(agentId)) {
            return MCPResponse::error(
                request.requestId,
                QString("Agent not found: %1").arg(agentId)
            );
        }
    }
    
    // Get AuthProfile
    if (!adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqAgentRemove(
        agentIds,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["agent_ids"] = QJsonArray::fromStringList(agentIds);
    data["count"] = agentIds.count();
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse AgentHandler::handleUpdateConfig(const MCPRequest& request)
{
    QString agentId = request.params.value("agent_id").toString();
    
    if (agentId.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: agent_id"
        );
    }
    
    // Verify agent exists
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent) {
        return MCPResponse::error(
            request.requestId,
            QString("Agent not found: %1").arg(agentId)
        );
    }
    
    // Extract config parameters
    QJsonObject config;
    bool hasConfig = false;
    
    if (request.params.contains("sleep")) {
        int sleep = request.params.value("sleep").toInt();
        if (sleep < 0) {
            return MCPResponse::error(
                request.requestId,
                "Invalid sleep value (must be >= 0)"
            );
        }
        config["sleep"] = sleep;
        hasConfig = true;
    }
    
    if (request.params.contains("jitter")) {
        int jitter = request.params.value("jitter").toInt();
        if (jitter < 0 || jitter > 100) {
            return MCPResponse::error(
                request.requestId,
                "Invalid jitter value (must be 0-100)"
            );
        }
        config["jitter"] = jitter;
        hasConfig = true;
    }
    
    if (!hasConfig) {
        return MCPResponse::error(
            request.requestId,
            "No configuration parameters provided (sleep, jitter)"
        );
    }
    
    // Build command string
    QString command;
    if (config.contains("sleep") && config.contains("jitter")) {
        command = QString("sleep %1 %2").arg(config["sleep"].toInt()).arg(config["jitter"].toInt());
    } else if (config.contains("sleep")) {
        command = QString("sleep %1").arg(config["sleep"].toInt());
    } else if (config.contains("jitter")) {
        command = QString("sleep %1 %2").arg(agent->data.Sleep).arg(config["jitter"].toInt());
    }
    
    // Send command via agent's console
    if (!agent->Console) {
        return MCPResponse::error(
            request.requestId,
            QString("Agent '%1' has no console").arg(agentId)
        );
    }
    
    bool success = false;
    QMetaObject::invokeMethod(agent->Console, "SetInput", Qt::BlockingQueuedConnection, Q_ARG(QString, command));
    QMetaObject::invokeMethod(agent->Console, "processInput", Qt::BlockingQueuedConnection);
    success = true;
    
    if (!success) {
        return MCPResponse::error(
            request.requestId,
            QString("Failed to send command to agent %1").arg(agentId)
        );
    }
    
    QJsonObject data;
    data["agent_id"] = agentId;
    data["command"] = command;
    data["config"] = config;
    
    return MCPResponse::success(
        request.requestId,
        QString("Configuration update command sent to agent %1").arg(agentId),
        data
    );
}

MCPResponse AgentHandler::handleSetTag(const MCPRequest& request)
{
    // Support both single agent_id and agent_ids array
    QStringList agentIds;
    
    if (request.params.contains("agent_id")) {
        QString agentId = request.params.value("agent_id").toString();
        if (agentId.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_id"
            );
        }
        agentIds.append(agentId);
    } else if (request.params.contains("agent_ids")) {
        QJsonArray idsArray = request.params.value("agent_ids").toArray();
        if (idsArray.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_ids array"
            );
        }
        for (const QJsonValue& val : idsArray) {
            agentIds.append(val.toString());
        }
    } else {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: agent_id or agent_ids"
        );
    }
    
    QString tag = request.params.value("tag").toString();
    // Tag can be empty to clear tags
    
    // Verify agents exist
    for (const QString& agentId : agentIds) {
        if (!adaptixWidget->AgentsMap.contains(agentId)) {
            return MCPResponse::error(
                request.requestId,
                QString("Agent not found: %1").arg(agentId)
            );
        }
    }
    
    // Get AuthProfile
    if (!adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqAgentSetTag(
        agentIds,
        tag,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["agent_ids"] = QJsonArray::fromStringList(agentIds);
    data["tag"] = tag;
    data["count"] = agentIds.count();
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse AgentHandler::handleSetMark(const MCPRequest& request)
{
    // Support both single agent_id and agent_ids array
    QStringList agentIds;
    
    if (request.params.contains("agent_id")) {
        QString agentId = request.params.value("agent_id").toString();
        if (agentId.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_id"
            );
        }
        agentIds.append(agentId);
    } else if (request.params.contains("agent_ids")) {
        QJsonArray idsArray = request.params.value("agent_ids").toArray();
        if (idsArray.isEmpty()) {
            return MCPResponse::error(
                request.requestId,
                "Missing or invalid agent_ids array"
            );
        }
        for (const QJsonValue& val : idsArray) {
            agentIds.append(val.toString());
        }
    } else {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: agent_id or agent_ids"
        );
    }
    
    QString mark = request.params.value("mark").toString();
    // Mark can be empty to clear marks
    
    // Verify agents exist
    for (const QString& agentId : agentIds) {
        if (!adaptixWidget->AgentsMap.contains(agentId)) {
            return MCPResponse::error(
                request.requestId,
                QString("Agent not found: %1").arg(agentId)
            );
        }
    }
    
    // Get AuthProfile
    if (!adaptixWidget->GetProfile()) {
        return MCPResponse::error(
            request.requestId,
            "Client is not authenticated to server"
        );
    }
    
    QString message;
    bool ok = false;
    
    bool result = HttpReqAgentSetMark(
        agentIds,
        mark,
        *adaptixWidget->GetProfile(),
        &message,
        &ok
    );
    
    if (!result) {
        return MCPResponse::error(
            request.requestId,
            "HTTP request timeout"
        );
    }
    
    if (!ok) {
        return MCPResponse::error(
            request.requestId,
            message
        );
    }
    
    QJsonObject data;
    data["agent_ids"] = QJsonArray::fromStringList(agentIds);
    data["mark"] = mark;
    data["count"] = agentIds.count();
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse AgentHandler::handleDeleteTasks(const MCPRequest& request)
{
    QString agentId = request.params.value("agent_id").toString();
    
    if (agentId.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: agent_id"
        );
    }
    
    // Verify agent exists
    Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
    if (!agent) {
        return MCPResponse::error(
            request.requestId,
            QString("Agent not found: %1").arg(agentId)
        );
    }
    
    // Extract task IDs (support both single task_id and array task_ids)
    QStringList taskIds;
    
    if (request.params.contains("task_id")) {
        QString taskId = request.params.value("task_id").toString();
        if (!taskId.isEmpty()) {
            taskIds.append(taskId);
        }
    }
    
    if (request.params.contains("task_ids")) {
        QJsonArray taskIdsArray = request.params.value("task_ids").toArray();
        for (const QJsonValue& val : taskIdsArray) {
            QString taskId = val.toString();
            if (!taskId.isEmpty() && !taskIds.contains(taskId)) {
                taskIds.append(taskId);
            }
        }
    }
    
    if (taskIds.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: task_id or task_ids"
        );
    }
    
    // Delete tasks using Agent's TasksDelete method
    QString message = agent->TasksDelete(taskIds);
    
    QJsonObject data;
    data["agent_id"] = agentId;
    data["task_ids"] = QJsonArray::fromStringList(taskIds);
    data["count"] = taskIds.count();
    
    return MCPResponse::success(
        request.requestId,
        message.isEmpty() ? QString("Deleted %1 task(s)").arg(taskIds.count()) : message,
        data
    );
}

