#ifndef MCPBRIDGEWORKER_H
#define MCPBRIDGEWORKER_H

#include <QObject>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QMutex>
#include "MCPProtocol.h"

class AdaptixWidget;
class MCPCommandHandler;

class MCPBridgeWorker : public QObject {
    Q_OBJECT
    
public:
    explicit MCPBridgeWorker(AdaptixWidget* widget, int port = 9999, QObject* parent = nullptr);
    ~MCPBridgeWorker() override;
    
    bool isRunning() const { return wsServer != nullptr && wsServer->isListening(); }
    quint16 getPort() const;
    int getConnectionCount() const;
    void sendResponse(const MCP::MCPResponse& response);

Q_SIGNALS:
    void started(quint16 port);
    void stopped();
    void connectionEstablished(QString peerAddress);
    void connectionClosed();
    void commandExecuted(QString type, bool success);
    void errorOccurred(QString error);
    
public Q_SLOTS:
    bool start();
    void stop();
    
private Q_SLOTS:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    
private:
    MCP::MCPResponse processRequest(const MCP::MCPRequest& request);
    MCP::MCPResponse handleBuiltinCommand(const MCP::MCPRequest& request);
    
    QWebSocketServer* wsServer;
    QWebSocket* mcpConnection;
    AdaptixWidget* adaptixWidget;
    MCPCommandHandler* commandHandler;
    int listenPort;
    mutable QMutex connectionMutex;
};

#endif
