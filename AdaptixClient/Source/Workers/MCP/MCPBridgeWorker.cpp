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
    // Use a local pointer to avoid race conditions during destruction
    QWebSocketServer* serverToKill = nullptr;
    QWebSocket* connToKill = nullptr;

    {
        QMutexLocker locker(&connectionMutex);
        serverToKill = wsServer;
        connToKill = mcpConnection;
        wsServer = nullptr;
        mcpConnection = nullptr;
    }

    if (connToKill) {
        connToKill->close();
        delete connToKill;
    }

    if (serverToKill) {
        serverToKill->close();
        delete serverToKill;
    }
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
    QMutexLocker locker(&connectionMutex);
    
    if (mcpConnection) {
        mcpConnection->close();
        mcpConnection->deleteLater();
        mcpConnection = nullptr;
    }
    
    if (wsServer) {
        wsServer->close();
        wsServer->deleteLater();
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
        // Allow new connection to supersede the old one
        disconnect(mcpConnection, nullptr, this, nullptr);
        mcpConnection->close(QWebSocketProtocol::CloseCodeNormal, "New connection replacing old one");
        mcpConnection->deleteLater();
        mcpConnection = nullptr;
    }
    
    mcpConnection = socket;
    mcpConnection->setParent(this); // Reparent to worker to manage lifecycle independently of server
    
    connect(mcpConnection, &QWebSocket::textMessageReceived, this, &MCPBridgeWorker::onTextMessageReceived);
    connect(mcpConnection, &QWebSocket::disconnected, this, &MCPBridgeWorker::onDisconnected);
    connect(mcpConnection, &QWebSocket::errorOccurred, this, &MCPBridgeWorker::onSocketError);
    
    Q_EMIT connectionEstablished(mcpConnection->peerAddress().toString());
}

void MCPBridgeWorker::onTextMessageReceived(const QString& message)
{
    // Signal that AI is busy processing a request
    Q_EMIT activityStatusChanged(true);

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
    // Ignore RemoteHostClosedError as it's a normal closure
    if (error == QAbstractSocket::RemoteHostClosedError)
        return;

    // Filter out generic "Unknown error" which often happens during teardown
    if (error == QAbstractSocket::UnknownSocketError && mcpConnection && mcpConnection->errorString() == "Unknown error")
        return;

    QMutexLocker locker(&connectionMutex);
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
    QMetaObject::invokeMethod(this, [this, response]() {
        internalSendResponse(response);
    }, Qt::QueuedConnection);
}

void MCPBridgeWorker::internalSendResponse(const MCP::MCPResponse& response)
{
    QMutexLocker locker(&connectionMutex);
    if (mcpConnection && mcpConnection->isValid()) {
        QJsonDocument doc(response.toJson());
        mcpConnection->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    
    // AI has responded, so it's back to standby/idle
    Q_EMIT activityStatusChanged(false);
}

void MCPBridgeWorker::sendMessage(const QString& type, const QJsonObject& params)
{
    QMetaObject::invokeMethod(this, "internalSendMessage", Qt::QueuedConnection, 
                           Q_ARG(QString, type), Q_ARG(QJsonObject, params));
}

void MCPBridgeWorker::internalSendMessage(const QString& type, const QJsonObject& params)
{
    QMutexLocker locker(&connectionMutex);
    if (!mcpConnection)
        return;

    QJsonObject json;
    json["version"] = MCP::PROTOCOL_VERSION;
    json["type"] = type;
    json["params"] = params;

    QJsonDocument doc(json);
    mcpConnection->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}
