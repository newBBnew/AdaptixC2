#ifndef ADAPTIX_MCP_TARGETSHANDLER_H
#define ADAPTIX_MCP_TARGETSHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

class AdaptixWidget;

class TargetsHandler : public IMCPCommandHandler {
public:
    explicit TargetsHandler(AdaptixWidget* widget);
    ~TargetsHandler() override = default;
    
    QString getCommandType() const override { return "targets"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Manage targets (list, create, edit, remove, set tag)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    MCP::MCPResponse handleListTargets(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_TARGETSHANDLER_H

