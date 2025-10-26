#include "TargetsHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <QJsonArray>

using namespace MCP;

TargetsHandler::TargetsHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse TargetsHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "list") {
        return handleListTargets(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown targets command: %1").arg(command)
        );
    }
}

MCPResponse TargetsHandler::handleListTargets(const MCPRequest& request)
{
    Q_UNUSED(request);
    
    QJsonArray targetsArray;
    
    for (const TargetData& target : adaptixWidget->Targets) {
        QJsonObject targetObj;
        targetObj["target_id"] = target.TargetId;
        targetObj["computer"] = target.Computer;
        targetObj["domain"] = target.Domain;
        targetObj["address"] = target.Address;
        targetObj["tag"] = target.Tag;
        targetObj["os"] = target.Os;
        targetObj["os_desc"] = target.OsDesc;
        targetObj["date"] = target.Date;
        targetObj["info"] = target.Info;
        targetObj["alive"] = target.Alive;
        targetObj["agents"] = QJsonArray::fromStringList(target.Agents);
        
        targetsArray.append(targetObj);
    }
    
    QJsonObject data;
    data["targets"] = targetsArray;
    data["total"] = targetsArray.count();
    
    return MCPResponse::success(
        request.requestId,
        QString("Found %1 targets").arg(targetsArray.count()),
        data
    );
}

