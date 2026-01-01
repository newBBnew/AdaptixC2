#ifndef MCPPROTOCOL_H
#define MCPPROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace MCP {

const QString PROTOCOL_VERSION = "1.0";

namespace Commands {
    const QString EXECUTE_COMMAND = "execute_command";
    const QString GET_CONSOLE_OUTPUT = "get_console_output";
    const QString CLEAR_CONSOLE = "clear_console";
    const QString LIST_AGENTS = "list_agents";
    const QString GET_AGENT_INFO = "get_agent_info";
    const QString UPDATE_AGENT_CONFIG = "update_agent_config";
    const QString UPDATE_AGENT_METADATA = "update_agent_metadata";
    const QString LIST_LISTENERS = "list_listeners";
    const QString MANAGE_LISTENER = "manage_listener";
    const QString LIST_TASKS = "list_tasks";
    const QString GET_TASK_OUTPUT = "get_task_output";
    const QString DELETE_TASKS = "delete_tasks";
    const QString LIST_TUNNELS = "list_tunnels";
    const QString MANAGE_TUNNEL = "manage_tunnel";
    const QString LIST_TARGETS = "list_targets";
    const QString LIST_PIVOTS = "list_pivots";
    const QString LIST_COLLECTED_DATA = "list_collected_data";
    const QString CAPTURE_SCREENSHOT = "capture_screenshot";
    const QString GET_UI_INFO = "get_ui_info";
    const QString PING = "ping";
    const QString GET_VERSION = "get_version";
    const QString GET_CAPABILITIES = "get_capabilities";
}

namespace Status {
    const QString SUCCESS = "success";
    const QString ERROR = "error";
    const QString NOT_SUPPORTED = "not_supported";
    const QString INVALID_PARAMS = "invalid_params";
    const QString TIMEOUT = "timeout";
}

struct MCPRequest {
    QString version;
    QString type;
    QString requestId;
    QJsonObject params;
    
    static MCPRequest fromJson(const QJsonObject& json) {
        MCPRequest req;
        req.version = json["version"].toString(PROTOCOL_VERSION);
        req.type = json["type"].toString();
        req.requestId = json["request_id"].toString();
        req.params = json["params"].toObject();
        return req;
    }
    
    QJsonObject toJson() const {
        QJsonObject json;
        json["version"] = version;
        json["type"] = type;
        json["request_id"] = requestId;
        json["params"] = params;
        return json;
    }
    
    bool isValid() const {
        return !type.isEmpty() && !requestId.isEmpty();
    }
};

struct MCPResponse {
    QString version;
    QString requestId;
    QString status;
    QString message;
    QJsonObject data;
    
    static MCPResponse success(const QString& reqId, const QString& msg = "", 
                              const QJsonObject& data = QJsonObject()) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::SUCCESS;
        resp.message = msg;
        resp.data = data;
        return resp;
    }
    
    static MCPResponse error(const QString& reqId, const QString& msg) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::ERROR;
        resp.message = msg;
        return resp;
    }
    
    static MCPResponse notSupported(const QString& reqId, const QString& type) {
        MCPResponse resp;
        resp.version = PROTOCOL_VERSION;
        resp.requestId = reqId;
        resp.status = Status::NOT_SUPPORTED;
        resp.message = QString("Command type '%1' not supported").arg(type);
        return resp;
    }
    
    QJsonObject toJson() const {
        QJsonObject json;
        json["version"] = version;
        json["request_id"] = requestId;
        json["status"] = status;
        json["message"] = message;
        if (!data.isEmpty()) {
            json["data"] = data;
        }
        return json;
    }
};

}

#endif
