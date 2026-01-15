#include <UI/Widgets/MSFWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QCompleter>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QWebSocket>
#include <QSslConfiguration>
#include <QDateTime>
#include <QTimer>

namespace {
const bool kMsvVerboseLogs = false;
}

const QStringList MSFConsoleWidget::MSF_COMPLETER_COMMANDS = {
    "use ",
    "set ",
    "setg ",
    "unset ",
    "unsetg ",
    "show options",
    "show advanced",
    "show payloads",
    "show targets",
    "show encoders",
    "show nops",
    "show auxiliary",
    "show post",
    "show exploits",
    "show missing",
    "exploit",
    "exploit -j",
    "exploit -z",
    "check",
    "back",
    "info ",
    "search ",
    "sessions",
    "sessions -l",
    "sessions -i ",
    "sessions -k ",
    "jobs",
    "jobs -k ",
    "route ",
    "clearroute",
    "load ",
    "unload ",
    "resource ",
    "save",
    "setg ",
    "spool ",
    "threads ",
    "color ",
    "exit",
    "quit",
    "help "
};

MSFConsoleWidget::MSFConsoleWidget(const QString& project, Settings* settings, QWidget* parent) : QWidget(parent)
{
    m_settings = settings;

    // 从Settings加载MSF配置，如果Settings存在的话
    if (m_settings) {
        m_msfHost = m_settings->data.MSFHost;
        m_msfPort = m_settings->data.MSFPort;
        m_msfUser = m_settings->data.MSFUser;
        m_msfPassword = m_settings->data.MSFPassword;
        m_msfSSL = m_settings->data.MSFSSL;
    }

    createUI();
    setupCompleter();

    m_outputPollingTimer = new QTimer(this);
    connect(m_outputPollingTimer, &QTimer::timeout, this, &MSFConsoleWidget::fetchConsoleOutput);
    m_outputPollingTimer->setInterval(500);

    m_dock = new KDDockWidgets::QtWidgets::DockWidget(project + "-MSFConsole", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    m_dock->setWidget(this);
    m_dock->setTitle("MSF Console");
}

MSFConsoleWidget::~MSFConsoleWidget() {}

void MSFConsoleWidget::setToken(const QString& token)
{
    m_token = token;
}

void MSFConsoleWidget::setServerUrl(const QString& url)
{
    m_serverUrl = url;
}

void MSFConsoleWidget::createUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 状态显示区域
    statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(8);
    
    // MSF服务状态
    msfServiceLabel = new QLabel("MSF Service:");
    msfServiceLabel->setStyleSheet("font-weight: 600; color: #c7c7c7;");
    
    msfServiceStatusLabel = new QLabel("Stopped");
    msfServiceStatusLabel->setStyleSheet("color: #f44336; padding: 3px 10px; border: 1px solid #f44336; border-radius: 10px; font-weight: 600; font-size: 11px;");
    
    // RPC连接状态
    rpcConnectionLabel = new QLabel("RPC Connection:");
    rpcConnectionLabel->setStyleSheet("font-weight: 600; color: #c7c7c7;");
    
    rpcConnectionStatusLabel = new QLabel("Disconnected");
    rpcConnectionStatusLabel->setStyleSheet("color: #f44336; padding: 3px 10px; border: 1px solid #f44336; border-radius: 10px; font-weight: 600; font-size: 11px;");
    
    // 控制台状态
    consoleStatusLabel = new QLabel("Console:");
    consoleStatusLabel->setStyleSheet("font-weight: 600; color: #c7c7c7;");
    
    consoleStatusValueLabel = new QLabel("Not Created");
    consoleStatusValueLabel->setStyleSheet("color: #888; padding: 3px 10px; border: 1px solid #888; border-radius: 10px; font-weight: 600; font-size: 11px;");
    
    statusLayout->addWidget(msfServiceLabel);
    statusLayout->addWidget(msfServiceStatusLabel);
    statusLayout->addSpacing(15);
    statusLayout->addWidget(rpcConnectionLabel);
    statusLayout->addWidget(rpcConnectionStatusLabel);
    statusLayout->addSpacing(15);
    statusLayout->addWidget(consoleStatusLabel);
    statusLayout->addWidget(consoleStatusValueLabel);
    statusLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    mainLayout->addLayout(statusLayout);

    // 控制按钮区域
    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    const QString buttonStyle = "QPushButton { background-color: #1a1a1a; border: 1px solid %1; color: %1; padding: 6px 12px; border-radius: 6px; font-weight: 600; } QPushButton:hover { background-color: #232323; } QPushButton:pressed { background-color: #2b2b2b; } QPushButton:disabled { color: #666; border-color: #333; }";

    // MSF服务控制按钮
    startMsfButton = new QPushButton("Start MSF");
    startMsfButton->setStyleSheet(buttonStyle.arg("#4caf50"));
    connect(startMsfButton, &QPushButton::clicked, this, &MSFConsoleWidget::onStartMsf);

    stopMsfButton = new QPushButton("Stop MSF");
    stopMsfButton->setStyleSheet(buttonStyle.arg("#f44336"));
    connect(stopMsfButton, &QPushButton::clicked, this, &MSFConsoleWidget::onStopMsf);

    // RPC连接控制按钮
    connectRpcButton = new QPushButton("Connect RPC");
    connectRpcButton->setStyleSheet(buttonStyle.arg("#2196f3"));
    connect(connectRpcButton, &QPushButton::clicked, this, &MSFConsoleWidget::onConnectRpc);

    disconnectRpcButton = new QPushButton("Disconnect RPC");
    disconnectRpcButton->setStyleSheet(buttonStyle.arg("#ff9800"));
    connect(disconnectRpcButton, &QPushButton::clicked, this, &MSFConsoleWidget::onDisconnectRpc);

    // 控制台操作按钮
    newConsoleButton = new QPushButton("New Console");
    newConsoleButton->setStyleSheet(buttonStyle.arg("#9c27b0"));
    connect(newConsoleButton, &QPushButton::clicked, this, &MSFConsoleWidget::refreshConsole);

    clearButton = new QPushButton("Clear");
    clearButton->setStyleSheet(buttonStyle.arg("#8a8a8a"));
    connect(clearButton, &QPushButton::clicked, outputTextEdit, &QTextEdit::clear);

    toolbarLayout->addWidget(startMsfButton);
    toolbarLayout->addWidget(stopMsfButton);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(connectRpcButton);
    toolbarLayout->addWidget(disconnectRpcButton);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(newConsoleButton);
    toolbarLayout->addWidget(clearButton);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    mainLayout->addLayout(toolbarLayout);

    outputTextEdit = new QTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    outputTextEdit->setFont(FontManager::instance().getDefaultMonospaceFont());
    outputTextEdit->setStyleSheet("background-color: #121212; color: #d4d4d4; border: 1px solid #2a2a2a; border-radius: 6px; padding: 6px;");

    mainLayout->addWidget(outputTextEdit);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(4);

    inputLineEdit = new QLineEdit();
    inputLineEdit->setPlaceholderText("MSF Console (enter MSF commands)");
    inputLineEdit->setFont(FontManager::instance().getDefaultMonospaceFont());
    inputLineEdit->setStyleSheet("background-color: #181818; color: #e0e0e0; border: 1px solid #2a2a2a; border-radius: 6px; padding: 6px 8px;");

    sendButton = new QPushButton("Send");
    sendButton->setIcon(QIcon::fromTheme("system-run"));
    sendButton->setStyleSheet("QPushButton { background-color: #1a1a1a; border: 1px solid #2196f3; color: #2196f3; padding: 6px 14px; border-radius: 6px; font-weight: 600; } QPushButton:hover { background-color: #232323; } QPushButton:pressed { background-color: #2b2b2b; }");

    connect(inputLineEdit, &QLineEdit::returnPressed, this, &MSFConsoleWidget::sendCommand);
    connect(sendButton, &QPushButton::clicked, this, &MSFConsoleWidget::sendCommand);

    inputLayout->addWidget(inputLineEdit);
    inputLayout->addWidget(sendButton);

    mainLayout->addLayout(inputLayout);
}

void MSFConsoleWidget::setupCompleter()
{
    completer = new QCompleter(msfCommands, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    inputLineEdit->setCompleter(completer);
}

void MSFConsoleWidget::onConnected()
{
    logMessage("SUCCESS", "Console connected successfully");
    updateRpcConnectionStatus("Connected", "#4caf50");
    updateConsoleStatus("Ready", "#4caf50");
    connectWebSocket();
}

void MSFConsoleWidget::updateMsfServiceStatus(const QString& status, const QString& color)
{
    if (msfServiceStatusLabel) {
        msfServiceStatusLabel->setText(status);
        msfServiceStatusLabel->setStyleSheet(QString("color: %1; padding: 2px 8px; border: 1px solid %1; border-radius: 3px;").arg(color));
    }
    logMessage("STATUS", QString("MSF Service: %1").arg(status));
}

void MSFConsoleWidget::updateRpcConnectionStatus(const QString& status, const QString& color)
{
    if (rpcConnectionStatusLabel) {
        rpcConnectionStatusLabel->setText(status);
        rpcConnectionStatusLabel->setStyleSheet(QString("color: %1; padding: 2px 8px; border: 1px solid %1; border-radius: 3px;").arg(color));
    }
    logMessage("STATUS", QString("RPC Connection: %1").arg(status));
}

void MSFConsoleWidget::updateConsoleStatus(const QString& status, const QString& color)
{
    if (consoleStatusValueLabel) {
        consoleStatusValueLabel->setText(status);
        consoleStatusValueLabel->setStyleSheet(QString("color: %1; padding: 2px 8px; border: 1px solid %1; border-radius: 3px;").arg(color));
    }
    logMessage("STATUS", QString("Console: %1").arg(status));
}

void MSFConsoleWidget::logMessage(const QString& level, const QString& message)
{
    if (!kMsvVerboseLogs && (level == "DEBUG" || level == "STATUS")) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] [%2] %3").arg(timestamp, level, message);
    
    // 输出到控制台（带颜色）
    if (outputTextEdit) {
        QString color = "#d4d4d4";
        if (level == "ERROR") color = "#f44336";
        else if (level == "SUCCESS") color = "#4caf50";
        else if (level == "WARNING") color = "#ff9800";
        else if (level == "DEBUG") color = "#607d8b";
        else if (level == "INFO") color = "#2196f3";
        
        outputTextEdit->append(QString("<span style='color: %1;'>%2</span>").arg(color, logEntry));
    }
    
    // 输出到调试控制台
    if (kMsvVerboseLogs) {
        qDebug() << "[MSF]" << logEntry;
    }
}

void MSFConsoleWidget::onDisconnected()
{
    logMessage("WARNING", "Console disconnected");
    updateRpcConnectionStatus("Disconnected", "#f44336");
    updateConsoleStatus("Not Ready", "#f44336");
}

void MSFConsoleWidget::connectWebSocket()
{
    if (m_webSocket) {
        disconnectWebSocket();
    }

    m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    auto sslConfig = m_webSocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_webSocket->setSslConfiguration(sslConfig);
    m_webSocket->ignoreSslErrors();

    connect(m_webSocket, &QWebSocket::connected, this, &MSFConsoleWidget::onWsConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &MSFConsoleWidget::onWsDisconnected);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &MSFConsoleWidget::onWsMessage);

    QString wsUrl = m_serverUrl;
    if (wsUrl.startsWith("https://")) {
        wsUrl.replace(0, 5, "wss");
    } else if (wsUrl.startsWith("http://")) {
        wsUrl.replace(0, 4, "ws");
    } else if (!wsUrl.startsWith("ws://") && !wsUrl.startsWith("wss://")) {
        wsUrl = "ws://" + wsUrl;
    }
    wsUrl += "/api/msf/ws";
    QNetworkRequest request{QUrl(wsUrl)};
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_webSocket->open(request);
}

void MSFConsoleWidget::disconnectWebSocket()
{
    if (m_webSocket) {
        m_webSocket->close();
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }
}

void MSFConsoleWidget::onWsConnected()
{
    logMessage("SUCCESS", "WebSocket connected");
    updateRpcConnectionStatus("WebSocket Connected", "#4caf50");
    stopOutputPolling();
}

void MSFConsoleWidget::onWsDisconnected()
{
    logMessage("WARNING", "WebSocket disconnected");
    updateRpcConnectionStatus("WebSocket Disconnected", "#ff9800");
    startOutputPolling();
}

void MSFConsoleWidget::onWsMessage(const QByteArray& message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message, &error);
    if (error.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();

    if (type == "console_output") {
        QString consoleId = json["console_id"].toString();
        QString data = json["data"].toString();
        bool busy = json["busy"].toBool();
        onConsoleOutput(consoleId, data, busy);
    } else if (type == "server_log") {
        // 服务器日志通过 WebSocket 发送过来
        QString msg = json["message"].toString();
        logMessage("SERVER", msg);
    }
}

void MSFConsoleWidget::onConsoleOutput(const QString& consoleId, const QString& data, bool busy)
{
    if (consoleId != m_currentConsoleId) return;

    if (!data.isEmpty()) {
        outputTextEdit->append(data);
    }
}

void MSFConsoleWidget::sendCommand()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Not connected to server");
        QMessageBox::warning(this, "MSF", "Not connected to server");
        return;
    }

    if (m_currentConsoleId.isEmpty()) {
        logMessage("WARNING", "No active console. Creating one...");
        refreshConsole();
        return;
    }

    QString command = inputLineEdit->text();
    if (command.isEmpty()) {
        return;
    }

    logMessage("DEBUG", QString("Sending command: %1").arg(command));
    inputLineEdit->clear();

    // 只发送command字段，console_id在URL中
    QJsonObject json;
    json["command"] = command;
    QJsonDocument doc(json);

    QByteArray data = doc.toJson(QJsonDocument::Compact);
    QString url = QString("%1/api/msf/console/%2/write").arg(m_serverUrl).arg(m_currentConsoleId);

    QJsonObject response = HttpReq(url, data, m_token);

    if (response["ok"].toBool()) {
        logMessage("SUCCESS", QString("Command sent: %1").arg(command));
    } else {
        logMessage("ERROR", QString("Failed to send command: %1").arg(response["error"].toString()));
    }
}

void MSFConsoleWidget::onStartMsf()
{
    logMessage("INFO", "=== Starting MSF Service ===");

    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Not connected to server");
        QMessageBox::warning(this, "MSF", "Not connected to server");
        return;
    }

    updateMsfServiceStatus("Starting...", "#ff9800");

    QString url = QString("%1/api/msf/controller/start").arg(m_serverUrl);

    QJsonObject config;
    if (m_msfHost.isEmpty()) {
        config["host"] = QString("127.0.0.1");
    } else {
        config["host"] = m_msfHost;
    }
    
    if (m_msfPort > 0) {
        config["port"] = m_msfPort;
    } else {
        config["port"] = 55552;
    }
    
    if (m_msfUser.isEmpty()) {
        config["user"] = QString("msf");
    } else {
        config["user"] = m_msfUser;
    }
    
    if (m_msfPassword.isEmpty()) {
        config["password"] = QString("test123");
    } else {
        config["password"] = m_msfPassword;
    }
    
    config["ssl"] = m_msfSSL ? true : false;

    QJsonDocument doc(config);
    QString configData = doc.toJson(QJsonDocument::Compact);

    QJsonObject response = HttpReq(url, configData.toUtf8(), m_token);

    if (response["ok"].toBool()) {
        logMessage("SUCCESS", "MSF service started successfully");
        outputTextEdit->append("[+] " + response["output"].toString());
        updateMsfServiceStatus("Running", "#4caf50");
    } else {
        logMessage("ERROR", "Failed to start MSF service");
        if (!response["error"].toString().isEmpty()) {
            logMessage("ERROR", QString("Error: %1").arg(response["error"].toString()));
            outputTextEdit->append("[-] Error: " + response["error"].toString());
        }
        updateMsfServiceStatus("Failed", "#f44336");
    }
}

void MSFConsoleWidget::onStopMsf()
{
    logMessage("INFO", "=== Stopping MSF Service ===");

    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Not connected to server");
        QMessageBox::warning(this, "MSF", "Not connected to server");
        return;
    }

    updateMsfServiceStatus("Stopping...", "#ff9800");

    QString url = QString("%1/api/msf/controller/stop").arg(m_serverUrl);

    QJsonObject response = HttpReq(url, QByteArray(), m_token);

    if (response["ok"].toBool()) {
        logMessage("SUCCESS", "MSF service stopped successfully");
        outputTextEdit->append("[+] MSF service stopped");
        updateMsfServiceStatus("Stopped", "#f44336");
        updateRpcConnectionStatus("Disconnected", "#f44336");
        updateConsoleStatus("Not Created", "#888");
        
        m_currentConsoleId.clear();
    } else {
        logMessage("ERROR", "Failed to stop MSF service");
        outputTextEdit->append("[-] Failed to stop MSF service");
        updateMsfServiceStatus("Failed", "#f44336");
    }
}

void MSFConsoleWidget::onConnectRpc()
{
    logMessage("INFO", "=== Connecting to RPC ===");

    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Not connected to server");
        QMessageBox::warning(this, "MSF", "Not connected to server");
        return;
    }

    connectMsfApi();
}

void MSFConsoleWidget::onDisconnectRpc()
{
    logMessage("INFO", "=== Disconnecting from RPC ===");
    
    updateRpcConnectionStatus("Disconnecting...", "#ff9800");
    
    m_currentConsoleId.clear();
    updateConsoleStatus("Not Created", "#888");
    
    logMessage("SUCCESS", "RPC disconnected successfully");
    updateRpcConnectionStatus("Disconnected", "#f44336");
    
    disconnectWebSocket();
    stopOutputPolling();
}

void MSFConsoleWidget::connectMsfApi()
{
    logMessage("INFO", "=== Connecting to MSF API ===");

    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Not connected to server");
        return;
    }

    updateRpcConnectionStatus("Connecting...", "#ff9800");
    
    // 使用重试机制连接
    connectMsfApiWithRetry();
}

void MSFConsoleWidget::connectMsfApiWithRetry()
{
    static int retryCount = 0;
    const int maxRetries = 5;
    const int retryInterval = 2000; // 2秒

    QString url = QString("%1/api/msf/start").arg(m_serverUrl);
    QJsonObject response = HttpReq(url, QByteArray(), m_token);

    if (response["ok"].toBool()) {
        logMessage("SUCCESS", "MSF API connected successfully");
        updateRpcConnectionStatus("Connected", "#4caf50");
        refreshConsole();
        retryCount = 0; // 重置计数器
    } else {
        retryCount++;
        if (retryCount < maxRetries) {
            logMessage("INFO", QString("MSF API connection failed, retrying in %1ms (%2/%3)")
                       .arg(retryInterval).arg(retryCount).arg(maxRetries));
            QTimer::singleShot(retryInterval, this, &MSFConsoleWidget::connectMsfApiWithRetry);
        } else {
            logMessage("ERROR", QString("MSF API connection failed after %1 attempts: %2")
                       .arg(maxRetries).arg(response["error"].toString()));
            updateRpcConnectionStatus("Failed", "#f44336");
            retryCount = 0; // 重置计数器
        }
    }
}

void MSFConsoleWidget::refreshConsole()
{
    logMessage("INFO", "=== Creating MSF Console ===");

    if (m_token.isEmpty() || m_serverUrl.isEmpty()) {
        logMessage("ERROR", "Cannot create console: not connected to server");
        return;
    }

    updateConsoleStatus("Creating...", "#ff9800");

    QString url = QString("%1/api/msf/console/create").arg(m_serverUrl);

    QJsonObject response = HttpReq(url, QByteArray(), m_token);

    if (response["ok"].toBool()) {
        m_currentConsoleId = response["id"].toString();
        m_lastOutputLength = 0;
        logMessage("SUCCESS", QString("Console created successfully with ID: %1").arg(m_currentConsoleId));
        updateConsoleStatus("Created", "#4caf50");
        onConnected();
    } else {
        logMessage("ERROR", QString("Failed to create console: %1").arg(response["error"].toString()));
        updateConsoleStatus("Failed", "#f44336");
    }
}

void MSFConsoleWidget::updateStatus(const QString& status)
{
    logMessage("DEBUG", QString("Legacy status update: %1").arg(status));
    
    if (status == "running") {
        updateMsfServiceStatus("Running", "#4caf50");
    } else if (status == "stopped") {
        updateMsfServiceStatus("Stopped", "#f44336");
    } else {
        updateMsfServiceStatus(status, "#888");
    }
}

void MSFConsoleWidget::checkMsfrpcdStatus()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty()) return;

    QJsonObject response = HttpReqGet(
        QString("%1/api/msf/controller/status").arg(m_serverUrl),
        m_token
    );

    if (response["ok"].toBool()) {
        QString status = response["status"].toString();
        updateStatus(status);
    }
}

void MSFConsoleWidget::onMsfrpcdStatus(const QJsonObject& response)
{
    Q_UNUSED(response)
    // 此方法保留用于 WebSocket 状态更新
    if (response["ok"].toBool()) {
        QString status = response["status"].toString();
        updateStatus(status);
    }
}

MSFSessionsWidget::MSFSessionsWidget(const QString& project, QWidget* parent) : QWidget(parent)
{
    createUI();

    m_dock = new KDDockWidgets::QtWidgets::DockWidget(project + "-MSFSessions", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    m_dock->setWidget(this);
    m_dock->setTitle("MSF Sessions");
}

MSFSessionsWidget::~MSFSessionsWidget() {}

void MSFSessionsWidget::setToken(const QString& token)
{
    m_token = token;
}

void MSFSessionsWidget::setServerUrl(const QString& url)
{
    m_serverUrl = url;
}

void MSFSessionsWidget::createUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(4);

    refreshButton = new QPushButton("Refresh");
    refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    connect(refreshButton, &QPushButton::clicked, this, &MSFSessionsWidget::onRefresh);

    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    mainLayout->addLayout(toolbarLayout);

    sessionsTable = new QTableWidget();
    sessionsTable->setColumnCount(6);
    sessionsTable->setHorizontalHeaderLabels({"ID", "Type", "Info", "Host", "Arch", ""});
    sessionsTable->horizontalHeader()->setStretchLastSection(true);
    sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sessionsTable->setStyleSheet("background-color: #2d2d2d;");

    mainLayout->addWidget(sessionsTable);
}

void MSFSessionsWidget::onSessionNew(const QJsonObject& session)
{
    SessionItem item;
    item.id = session["id"].toString();
    item.type = session["type"].toString();
    item.info = session["info"].toString();
    item.host = session["session_host"].toString();

    m_sessions[item.id] = item;
    updateTable(QJsonObject());
}

void MSFSessionsWidget::onSessionClosed(const QString& sessionId)
{
    m_sessions.remove(sessionId);
    updateTable(QJsonObject());
}

void MSFSessionsWidget::onSessionsUpdate(const QJsonObject& sessions)
{
    m_sessions.clear();
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        QJsonObject session = it.value().toObject();
        SessionItem item;
        item.id = it.key();
        item.type = session["type"].toString();
        item.info = session["info"].toString();
        item.host = session["session_host"].toString();
        m_sessions[item.id] = item;
    }
    updateTable(sessions);
}

void MSFSessionsWidget::onRefresh()
{
    refreshSessions();
}

void MSFSessionsWidget::onInteract(const QString& sessionId)
{
    bool ok;
    QString command = QInputDialog::getText(this, "Session Interaction",
        QString("Enter command for session %1:").arg(sessionId),
        QLineEdit::Normal, "", &ok);

    if (ok && !command.isEmpty()) {
        QJsonObject jsonData;
        jsonData["command"] = command;

        HttpReq(
            QString("%1/api/msf/sessions/%2/interact").arg(m_serverUrl).arg(sessionId),
            QJsonDocument(jsonData).toJson(QJsonDocument::Compact),
            m_token
        );
    }
}

void MSFSessionsWidget::onKill(const QString& sessionId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Kill Session",
        QString("Kill session %1?").arg(sessionId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        HttpReq(
            QString("%1/api/msf/sessions/%2/kill").arg(m_serverUrl).arg(sessionId),
            QByteArray(),
            m_token
        );
    }
}

void MSFSessionsWidget::updateTable(const QJsonObject& sessions)
{
    Q_UNUSED(sessions)

    sessionsTable->setRowCount(m_sessions.count());

    int row = 0;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        const SessionItem& item = it.value();

        sessionsTable->setItem(row, 0, new QTableWidgetItem(item.id));
        sessionsTable->setItem(row, 1, new QTableWidgetItem(item.type));
        sessionsTable->setItem(row, 2, new QTableWidgetItem(item.info));
        sessionsTable->setItem(row, 3, new QTableWidgetItem(item.host));
        sessionsTable->setItem(row, 4, new QTableWidgetItem("x64"));

        QPushButton* interactBtn = new QPushButton("Interact");
        connect(interactBtn, &QPushButton::clicked, this, [this, item]() { onInteract(item.id); });

        QPushButton* killBtn = new QPushButton("Kill");
        killBtn->setStyleSheet("color: #f44336;");
        connect(killBtn, &QPushButton::clicked, this, [this, item]() { onKill(item.id); });

        QWidget* buttonWidget = new QWidget();
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->addWidget(interactBtn);
        buttonLayout->addWidget(killBtn);
        buttonLayout->setContentsMargins(0, 0, 0, 0);

        sessionsTable->setCellWidget(row, 5, buttonWidget);

        row++;
    }

    sessionsTable->resizeColumnsToContents();
}

void MSFSessionsWidget::refreshSessions()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty()) return;

    QJsonObject response = HttpReq(
        QString("%1/api/msf/sessions").arg(m_serverUrl),
        QByteArray(),
        m_token
    );

    if (response["ok"].toBool()) {
        onSessionsUpdate(response["sessions"].toObject());
    }
}

void MSFConsoleWidget::startOutputPolling()
{
    if (m_outputPollingTimer && !m_outputPollingTimer->isActive()) {
        m_lastOutputLength = 0;
        m_outputPollingTimer->start();
        logMessage("DEBUG", "Started output polling");
    }
}

void MSFConsoleWidget::stopOutputPolling()
{
    if (m_outputPollingTimer && m_outputPollingTimer->isActive()) {
        m_outputPollingTimer->stop();
        logMessage("DEBUG", "Stopped output polling");
    }
}

void MSFConsoleWidget::fetchConsoleOutput()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty() || m_currentConsoleId.isEmpty()) {
        return;
    }

    QString url = QString("%1/api/msf/console/%2/read").arg(m_serverUrl).arg(m_currentConsoleId);
    QJsonObject response = HttpReqGet(url, m_token);

    if (response["ok"].toBool()) {
        QString data = response["data"].toString();
        bool busy = response["busy"].toBool();

        if (!data.isEmpty()) {
            outputTextEdit->append(data);
            m_lastOutputLength += data.length();
        }
    }
}
