#ifndef ADAPTIX_MCP_PIVOTSHANDLER_H
#define ADAPTIX_MCP_PIVOTSHANDLER_H

#include "../MCPCommandHandler.h"
#include <QObject>

class AdaptixWidget;

class PivotsHandler : public IMCPCommandHandler {
public:
    explicit PivotsHandler(AdaptixWidget* widget);
    ~PivotsHandler() override = default;
    
    QString getCommandType() const override { return "pivots"; }
    QString getVersion() const override { return "1.0"; }
    bool isSupported() const override { return true; }
    QString getDescription() const override { 
        return "Manage pivots (list pivot connections)"; 
    }
    MCP::MCPResponse handle(const MCP::MCPRequest& request) override;

private:
    AdaptixWidget* adaptixWidget;
    
    MCP::MCPResponse handleListPivots(const MCP::MCPRequest& request);
};

#endif // ADAPTIX_MCP_PIVOTSHANDLER_H

