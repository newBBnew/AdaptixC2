#include "TunnelHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <QJsonArray>
#include <QJsonDocument>

using namespace MCP;

TunnelHandler::TunnelHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse TunnelHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "list") {
        return handleListTunnels(request);
    } else if (command == "get_info") {
        return handleGetTunnelInfo(request);
    } else if (command == "create") {
        return handleCreateTunnel(request);
    } else if (command == "stop") {
        return handleStopTunnel(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown tunnel command: %1").arg(command)
        );
    }
}

MCPResponse TunnelHandler::handleListTunnels(const MCPRequest& request)
{
    Q_UNUSED(request);
    
    QJsonArray tunnelsArray;
    
    // Iterate through all tunnels
    for (const TunnelData& tunnel : adaptixWidget->Tunnels) {
        QJsonObject tunnelObj;
        tunnelObj["tunnel_id"] = tunnel.TunnelId;
        tunnelObj["agent_id"] = tunnel.AgentId;
        tunnelObj["computer"] = tunnel.Computer;
        tunnelObj["username"] = tunnel.Username;
        tunnelObj["process"] = tunnel.Process;
        tunnelObj["type"] = tunnel.Type;
        tunnelObj["info"] = tunnel.Info;
        tunnelObj["interface"] = tunnel.Interface;
        tunnelObj["port"] = tunnel.Port;
        tunnelObj["client"] = tunnel.Client;
        tunnelObj["fport"] = tunnel.Fport;
        tunnelObj["fhost"] = tunnel.Fhost;
        
        tunnelsArray.append(tunnelObj);
    }
    
    QJsonObject data;
    data["tunnels"] = tunnelsArray;
    data["total"] = tunnelsArray.count();
    
    return MCPResponse::success(
        request.requestId,
        QString("Found %1 tunnels").arg(tunnelsArray.count()),
        data
    );
}

MCPResponse TunnelHandler::handleGetTunnelInfo(const MCPRequest& request)
{
    QString tunnelId = request.params.value("tunnel_id").toString();
    
    if (tunnelId.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: tunnel_id"
        );
    }
    
    // Find the tunnel
    for (const TunnelData& tunnel : adaptixWidget->Tunnels) {
        if (tunnel.TunnelId == tunnelId) {
            QJsonObject tunnelObj;
            tunnelObj["tunnel_id"] = tunnel.TunnelId;
            tunnelObj["agent_id"] = tunnel.AgentId;
            tunnelObj["computer"] = tunnel.Computer;
            tunnelObj["username"] = tunnel.Username;
            tunnelObj["process"] = tunnel.Process;
            tunnelObj["type"] = tunnel.Type;
            tunnelObj["info"] = tunnel.Info;
            tunnelObj["interface"] = tunnel.Interface;
            tunnelObj["port"] = tunnel.Port;
            tunnelObj["client"] = tunnel.Client;
            tunnelObj["fport"] = tunnel.Fport;
            tunnelObj["fhost"] = tunnel.Fhost;
            
            QJsonObject data;
            data["tunnel"] = tunnelObj;
            
            return MCPResponse::success(
                request.requestId,
                QString("Tunnel info for %1").arg(tunnelId),
                data
            );
        }
    }
    
    return MCPResponse::error(
        request.requestId,
        QString("Tunnel not found: %1").arg(tunnelId)
    );
}

MCPResponse TunnelHandler::handleCreateTunnel(const MCPRequest& request)
{
    QString tunnelType = request.params.value("tunnel_type").toString();
    QString config = request.params.value("config").toString();
    
    if (tunnelType.isEmpty() || config.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameters: tunnel_type, config"
        );
    }
    
    // Validate tunnel type
    QStringList validTypes = {"socks4", "socks5", "lportfwd", "rportfwd"};
    if (!validTypes.contains(tunnelType)) {
        return MCPResponse::error(
            request.requestId,
            QString("Invalid tunnel_type: %1. Valid types: %2").arg(tunnelType).arg(validTypes.join(", "))
        );
    }
    
    // Validate JSON config
    QJsonParseError parseError;
    QJsonDocument::fromJson(config.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return MCPResponse::error(
            request.requestId,
            QString("Invalid JSON config: %1").arg(parseError.errorString())
        );
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
    
    bool result = HttpReqTunnelStartServer(
        tunnelType,
        config.toUtf8(),
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
    data["tunnel_type"] = tunnelType;
    data["message"] = message;
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

MCPResponse TunnelHandler::handleStopTunnel(const MCPRequest& request)
{
    QString tunnelId = request.params.value("tunnel_id").toString();
    
    if (tunnelId.isEmpty()) {
        return MCPResponse::error(
            request.requestId,
            "Missing required parameter: tunnel_id"
        );
    }
    
    // Check if tunnel exists
    bool found = false;
    for (const TunnelData& tunnel : adaptixWidget->Tunnels) {
        if (tunnel.TunnelId == tunnelId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        return MCPResponse::error(
            request.requestId,
            QString("Tunnel not found: %1").arg(tunnelId)
        );
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
    
    bool result = HttpReqTunnelStop(
        tunnelId,
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
    data["tunnel_id"] = tunnelId;
    
    return MCPResponse::success(
        request.requestId,
        message,
        data
    );
}

