#include "PivotsHandler.h"
#include <UI/Widgets/AdaptixWidget.h>
#include <QJsonArray>

using namespace MCP;

PivotsHandler::PivotsHandler(AdaptixWidget* widget)
    : adaptixWidget(widget)
{
    Q_ASSERT(widget != nullptr);
}

MCPResponse PivotsHandler::handle(const MCPRequest& request)
{
    QString command = request.params.value("command").toString();
    
    if (command == "list") {
        return handleListPivots(request);
    } else {
        return MCPResponse::error(
            request.requestId,
            QString("Unknown pivots command: %1").arg(command)
        );
    }
}

MCPResponse PivotsHandler::handleListPivots(const MCPRequest& request)
{
    Q_UNUSED(request);
    
    QJsonArray pivotsArray;
    
    for (auto it = adaptixWidget->Pivots.constBegin(); 
         it != adaptixWidget->Pivots.constEnd(); ++it) {
        const PivotData& pivot = it.value();
        
        QJsonObject pivotObj;
        pivotObj["pivot_id"] = pivot.PivotId;
        pivotObj["pivot_name"] = pivot.PivotName;
        pivotObj["parent_agent_id"] = pivot.ParentAgentId;
        pivotObj["child_agent_id"] = pivot.ChildAgentId;
        
        pivotsArray.append(pivotObj);
    }
    
    QJsonObject data;
    data["pivots"] = pivotsArray;
    data["total"] = pivotsArray.count();
    
    return MCPResponse::success(
        request.requestId,
        QString("Found %1 pivots").arg(pivotsArray.count()),
        data
    );
}

