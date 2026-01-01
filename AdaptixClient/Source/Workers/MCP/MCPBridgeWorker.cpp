#include <Workers/MCP/MCPBridgeWorker.h>
#include <Workers/MCP/MCPCommandHandler.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <QJsonDocument>

MCPBridgeWorker::MCPBridgeWorker(AdaptixWidget* widget, int port, QObject* parent)
    : QObject(parent)
    , wsServer(nullptr)
    , mcpConnection(nullptr)
    , adaptixWidget(widget)
    , commandHandler(nullptr)
    , listenPort(port)
{
    commandHandler = new MCPCommandHandler(widget, this);
}

MCPBridgeWorker::~MCPBridgeWorker()
{
    stop();
}

quint16 MCPBridgeWorker::getPort() const
{
    return wsServer ? wsServer->serverPort() : 0;
}

int MCPBridgeWorker::getConnectionCount() const
{
    QMutexLocker locker(&connectionMutex);
    return mcpConnection ? 1 : 0;
}

bool MCPBridgeWorker::start()
{
    if (wsServer && wsServer->isListening())
        return true;

    wsServer = new QWebSocketServer("MCP Bridge", QWebSocketServer::NonSecureMode, this);
    
    if (!wsServer->listen(QHostAddress::LocalHost, listenPort)) {
        Q_EMIT errorOccurred(QString("Failed to start MCP Bridge on port %1: %2")
                          .arg(listenPort).arg(wsServer->errorString()));
        delete wsServer;
        wsServer = nullptr;
        return false;
    }
    
    connect(wsServer, &QWebSocketServer::newConnection, this, &MCPBridgeWorker::onNewConnection);
    
    Q_EMIT started(wsServer->serverPort());
    return true;
}

void MCPBridgeWorker::stop()
{
    if (mcpConnection) {
        mcpConnection->close();
        mcpConnection = nullptr;
    }
    
    if (wsServer) {
        wsServer->close();
        delete wsServer;
        wsServer = nullptr;
    }
    
    Q_EMIT stopped();
}

void MCPBridgeWorker::onNewConnection()
{
    QWebSocket* socket = wsServer->nextPendingConnection();
    if (!socket)
        return;
    
    QMutexLocker locker(&connectionMutex);
    
    if (mcpConnection) {
        socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Only one connection allowed");
        socket->deleteLater();
        return;
    }
    
    mcpConnection = socket;
    
    connect(mcpConnection, &QWebSocket::textMessageReceived, this, &MCPBridgeWorker::onTextMessageReceived);
    connect(mcpConnection, &QWebSocket::disconnected, this, &MCPBridgeWorker::onDisconnected);
    connect(mcpConnection, &QWebSocket::errorOccurred, this, &MCPBridgeWorker::onSocketError);
    
    Q_EMIT connectionEstablished(mcpConnection->peerAddress().toString());
}

void MCPBridgeWorker::onTextMessageReceived(const QString& message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        MCP::MCPResponse resp = MCP::MCPResponse::error("", "Invalid JSON: " + error.errorString());
        sendResponse(resp);
        return;
    }
    
    MCP::MCPRequest request = MCP::MCPRequest::fromJson(doc.object());
    
    if (!request.isValid()) {
        MCP::MCPResponse resp = MCP::MCPResponse::error(request.requestId, "Invalid request format");
        sendResponse(resp);
        return;
    }
    
    MCP::MCPResponse response = processRequest(request);
    if (response.status != MCP::Status::DEFERRED) {
        sendResponse(response);
    }
    
    Q_EMIT commandExecuted(request.type, response.status == MCP::Status::SUCCESS || response.status == MCP::Status::DEFERRED);
}

void MCPBridgeWorker::onDisconnected()
{
    QMutexLocker locker(&connectionMutex);
    if (mcpConnection) {
        mcpConnection->deleteLater();
        mcpConnection = nullptr;
    }
    Q_EMIT connectionClosed();
}

void MCPBridgeWorker::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (mcpConnection) {
        Q_EMIT errorOccurred(mcpConnection->errorString());
    }
}

MCP::MCPResponse MCPBridgeWorker::processRequest(const MCP::MCPRequest& request)
{
    if (request.type == MCP::Commands::PING) {
        return MCP::MCPResponse::success(request.requestId, "pong");
    }
    
    if (request.type == MCP::Commands::GET_VERSION) {
        QJsonObject data;
        data["version"] = MCP::PROTOCOL_VERSION;
        data["framework"] = FRAMEWORK_VERSION;
        return MCP::MCPResponse::success(request.requestId, "", data);
    }
    
    return commandHandler->handleCommand(request);
}

void MCPBridgeWorker::sendResponse(const MCP::MCPResponse& response)
{
    QMutexLocker locker(&connectionMutex);
    if (!mcpConnection)
        return;
    
    QJsonDocument doc(response.toJson());
    mcpConnection->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}
