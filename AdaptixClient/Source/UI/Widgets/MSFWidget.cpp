#include <UI/Widgets/MSFWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>
#include <Utils/Convert.h>

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
#include <QKeyEvent>
#include <QRegularExpression>
#include <QUrl>

namespace {
const bool kMsvVerboseLogs = false;

QColor statusAccentFor(const QWidget* widget, const QString& status)
{
    if (!widget) {
        return QColor();
    }

    const QString normalized = status.toLower();
    const QPalette pal = widget->palette();

    if (normalized.contains("running") || normalized.contains("connected") || normalized.contains("ready") || normalized.contains("created")) {
        return pal.color(QPalette::Highlight);
    }
    if (normalized.contains("starting") || normalized.contains("stopping") || normalized.contains("connecting") || normalized.contains("disconnecting") || normalized.contains("creating")) {
        return pal.color(QPalette::Link);
    }
    if (normalized.contains("failed") || normalized.contains("stopped") || normalized.contains("disconnected") || normalized.contains("not")) {
        return pal.color(QPalette::Mid);
    }

    return pal.color(QPalette::Text);
}

QString statusPillStyle(const QWidget* widget, const QColor& accent)
{
    const QPalette pal = widget ? widget->palette() : QPalette();
    const QColor base = pal.color(QPalette::Base);
    const QColor border = accent.isValid() ? accent : pal.color(QPalette::Mid);

    return QString("padding: 3px 10px; border-radius: 10px; font-weight: 600; font-size: 11px; background-color: %1; border: 1px solid %2; color: %2;")
        .arg(base.name(QColor::HexRgb), border.name(QColor::HexRgb));
}

void applyStatusPill(QLabel* label, const QString& status, const QString& color)
{
    if (!label) {
        return;
    }

    const QColor accent = color.isEmpty() ? statusAccentFor(label, status) : QColor(color);
    label->setText(status);
    label->setStyleSheet(statusPillStyle(label, accent));
}

QColor logColorFor(const QTextEdit* edit, const QString& level)
{
    const QPalette pal = edit ? edit->palette() : QPalette();
    if (level == "ERROR") {
        return pal.color(QPalette::BrightText);
    }
    if (level == "SUCCESS") {
        return pal.color(QPalette::Highlight);
    }
    if (level == "WARNING") {
        return pal.color(QPalette::Link);
    }
    if (level == "DEBUG") {
        return pal.color(QPalette::Mid);
    }
    return pal.color(QPalette::Text);
}

QIcon themedIcon(const QIcon& icon, const QWidget* widget)
{
    const QColor color = widget ? widget->palette().color(QPalette::ButtonText) : QColor(200, 200, 200);
    return RecolorIcon(icon, color);
}
}

void MSFConsoleWidget::updateCompleterModel()
{
    QStringList items = m_completionCache.values();
    items.sort(Qt::CaseInsensitive);
    if (m_completerModel) {
        m_completerModel->setStringList(items);
    }
}

void MSFConsoleWidget::updateCompleterPopup(const QString& text)
{
    if (!completer || !m_completerModel) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        completer->popup()->hide();
        return;
    }

    int lastSpace = text.lastIndexOf(' ');
    if (lastSpace < 0) {
        m_completionPrefixBase.clear();
        completer->setCompletionPrefix(text.trimmed());
    } else {
        m_completionPrefixBase = text.left(lastSpace + 1);
        completer->setCompletionPrefix(text.mid(lastSpace + 1).trimmed());
    }

    if (completer->completionPrefix().isEmpty()) {
        completer->popup()->hide();
        return;
    }

    completer->complete();
}

void MSFConsoleWidget::addCompletionCandidate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (!m_completionCache.contains(trimmed)) {
        m_completionCache.insert(trimmed);
        updateCompleterModel();
    }
}

void MSFConsoleWidget::addHistoryEntry(const QString& command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_commandHistory.removeAll(trimmed);
    m_commandHistory.prepend(trimmed);
    if (m_commandHistory.size() > 200) {
        m_commandHistory = m_commandHistory.mid(0, 200);
    }
    m_historyIndex = -1;
}

void MSFConsoleWidget::extractOutputCompletions(const QString& output)
{
    const QRegularExpression moduleRegex(R"((?:auxiliary|exploit|post|payload|encoder|nop)/[A-Za-z0-9_./-]+)");
    const QRegularExpression optionRegex(R"(\b[A-Z0-9_]{3,}\b)");

    auto moduleIt = moduleRegex.globalMatch(output);
    while (moduleIt.hasNext()) {
        const auto match = moduleIt.next();
        addCompletionCandidate(match.captured(0));
    }

    auto optionIt = optionRegex.globalMatch(output);
    while (optionIt.hasNext()) {
        const auto match = optionIt.next();
        const QString token = match.captured(0);
        if (token.size() >= 3 && token.size() <= 32) {
            addCompletionCandidate(token);
        }
    }
}

void MSFConsoleWidget::preheatCompletions()
{
    if (m_preheatDone || m_token.isEmpty() || m_serverUrl.isEmpty()) {
        return;
    }

    const int maxAttempts = 5;
    if (m_preheatAttempts >= maxAttempts) {
        return;
    }

    m_preheatAttempts++;

    QJsonObject health = HttpReqGet(QString("%1/api/msf/health").arg(m_serverUrl), m_token);
    if (!health["ok"].toBool() || !health["healthy"].toBool()) {
        const int delayMs = 1500 * m_preheatAttempts;
        QTimer::singleShot(delayMs, this, &MSFConsoleWidget::preheatCompletions);
        return;
    }

    QJsonObject response = HttpReqGet(QString("%1/api/msf/modules").arg(m_serverUrl), m_token);
    if (!response["ok"].toBool()) {
        const int delayMs = 1500 * m_preheatAttempts;
        QTimer::singleShot(delayMs, this, &MSFConsoleWidget::preheatCompletions);
        return;
    }

    const QJsonArray modules = response["modules"].toArray();
    const int maxOptionModules = 10;
    const int maxPayloadModules = 5;
    int optionFetched = 0;
    int payloadFetched = 0;
    QSet<QString> seenTypes;

    const auto encode = [](const QString& value) {
        return QString::fromUtf8(QUrl::toPercentEncoding(value));
    };

    for (const auto& entry : modules) {
        const QString moduleName = entry.toString();
        if (moduleName.isEmpty()) {
            continue;
        }
        const int slashIndex = moduleName.indexOf('/');
        const QString moduleType = slashIndex > 0 ? moduleName.left(slashIndex) : QString();
        const QString modulePath = slashIndex > 0 ? moduleName.mid(slashIndex + 1) : moduleName;

        addCompletionCandidate(moduleName);
        addCompletionCandidate(QString("use %1").arg(moduleName));
        addCompletionCandidate(QString("info %1").arg(moduleName));

        if (!moduleType.isEmpty()) {
            if (!seenTypes.contains(moduleType)) {
                seenTypes.insert(moduleType);
                addCompletionCandidate(QString("show %1").arg(moduleType));
            }

            if (optionFetched < maxOptionModules) {
                const QString optionsUrl = QString("%1/api/msf/modules/options?type=%2&name=%3")
                                               .arg(m_serverUrl, encode(moduleType), encode(modulePath));
                QJsonObject optionsResponse = HttpReqGet(optionsUrl, m_token);
                if (optionsResponse["ok"].toBool()) {
                    const QJsonObject options = optionsResponse["options"].toObject();
                    for (auto it = options.begin(); it != options.end(); ++it) {
                        const QString optionName = it.key();
                        if (optionName.isEmpty()) {
                            continue;
                        }
                        addCompletionCandidate(optionName);
                        addCompletionCandidate(QString("set %1 ").arg(optionName));
                        addCompletionCandidate(QString("setg %1 ").arg(optionName));
                    }
                }
                optionFetched++;
            }

            if (payloadFetched < maxPayloadModules && moduleType == "exploit") {
                const QString payloadUrl = QString("%1/api/msf/modules/compatible_payloads?type=%2&name=%3")
                                               .arg(m_serverUrl, encode(moduleType), encode(modulePath));
                QJsonObject payloadResponse = HttpReqGet(payloadUrl, m_token);
                if (payloadResponse["ok"].toBool()) {
                    const QJsonArray payloads = payloadResponse["payloads"].toArray();
                    for (const auto& payloadEntry : payloads) {
                        const QString payloadName = payloadEntry.toString();
                        if (payloadName.isEmpty()) {
                            continue;
                        }
                        addCompletionCandidate(payloadName);
                        addCompletionCandidate(QString("set PAYLOAD %1").arg(payloadName));
                    }
                }
                payloadFetched++;
            }
        }

        if (optionFetched >= maxOptionModules && payloadFetched >= maxPayloadModules) {
            break;
        }
    }

    m_preheatDone = true;
}

MSFConsoleWidget::MSFConsoleWidget(const QString& project, Settings* settings, QWidget* parent, const bool createDock) : QWidget(parent)
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

    if (createDock) {
        m_dock = new KDDockWidgets::QtWidgets::DockWidget(project + "-MSFConsole", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        m_dock->setWidget(this);
        m_dock->setTitle("MSF Console");
    }
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
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto* topPanel = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(6);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);
    auto* titleLabel = new QLabel("Metasploit Console");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    topLayout->addLayout(headerLayout);

    statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(8);

    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSize(qMax(9, labelFont.pointSize() - 1));

    msfServiceLabel = new QLabel("MSF Service");
    msfServiceLabel->setFont(labelFont);

    msfServiceStatusLabel = new QLabel("Stopped");
    applyStatusPill(msfServiceStatusLabel, "Stopped", "");

    rpcConnectionLabel = new QLabel("RPC Connection");
    rpcConnectionLabel->setFont(labelFont);

    rpcConnectionStatusLabel = new QLabel("Disconnected");
    applyStatusPill(rpcConnectionStatusLabel, "Disconnected", "");

    consoleStatusLabel = new QLabel("Console");
    consoleStatusLabel->setFont(labelFont);

    consoleStatusValueLabel = new QLabel("Not Created");
    applyStatusPill(consoleStatusValueLabel, "Not Created", "");

    statusLayout->addWidget(msfServiceLabel);
    statusLayout->addWidget(msfServiceStatusLabel);
    statusLayout->addSpacing(12);
    statusLayout->addWidget(rpcConnectionLabel);
    statusLayout->addWidget(rpcConnectionStatusLabel);
    statusLayout->addSpacing(12);
    statusLayout->addWidget(consoleStatusLabel);
    statusLayout->addWidget(consoleStatusValueLabel);
    statusLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    topLayout->addLayout(statusLayout);

    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    startMsfButton = new QPushButton("Start MSF");
    connect(startMsfButton, &QPushButton::clicked, this, &MSFConsoleWidget::onStartMsf);

    stopMsfButton = new QPushButton("Stop MSF");
    connect(stopMsfButton, &QPushButton::clicked, this, &MSFConsoleWidget::onStopMsf);

    connectRpcButton = new QPushButton("Connect RPC");
    connect(connectRpcButton, &QPushButton::clicked, this, &MSFConsoleWidget::onConnectRpc);

    disconnectRpcButton = new QPushButton("Disconnect RPC");
    connect(disconnectRpcButton, &QPushButton::clicked, this, &MSFConsoleWidget::onDisconnectRpc);

    newConsoleButton = new QPushButton("New Console");
    connect(newConsoleButton, &QPushButton::clicked, this, &MSFConsoleWidget::refreshConsole);

    clearButton = new QPushButton("Clear");
    connect(clearButton, &QPushButton::clicked, outputTextEdit, &QTextEdit::clear);

    toolbarLayout->addWidget(startMsfButton);
    toolbarLayout->addWidget(stopMsfButton);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(connectRpcButton);
    toolbarLayout->addWidget(disconnectRpcButton);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(newConsoleButton);
    toolbarLayout->addWidget(clearButton);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    topLayout->addLayout(toolbarLayout);

    auto* consolePanel = new QWidget(this);
    auto* consoleLayout = new QVBoxLayout(consolePanel);
    consoleLayout->setContentsMargins(0, 0, 0, 0);
    consoleLayout->setSpacing(6);

    outputTextEdit = new QTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    outputTextEdit->setFont(FontManager::instance().getDefaultMonospaceFont());
    outputTextEdit->setProperty("TextEditStyle", "console");

    consoleLayout->addWidget(outputTextEdit);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    inputLineEdit = new QLineEdit();
    inputLineEdit->setPlaceholderText("MSF Console (enter MSF commands)");
    inputLineEdit->setFont(FontManager::instance().getDefaultMonospaceFont());
    inputLineEdit->setProperty("LineEditStyle", "console");

    sendButton = new QPushButton("Send");
    sendButton->setIcon(themedIcon(QIcon::fromTheme("system-run"), sendButton));

    connect(inputLineEdit, &QLineEdit::returnPressed, this, &MSFConsoleWidget::sendCommand);
    connect(sendButton, &QPushButton::clicked, this, &MSFConsoleWidget::sendCommand);

    inputLayout->addWidget(inputLineEdit);
    inputLayout->addWidget(sendButton);

    consoleLayout->addLayout(inputLayout);

    auto* mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->setHandleWidth(3);
    mainSplitter->addWidget(topPanel);
    mainSplitter->addWidget(consolePanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(mainSplitter);
}

void MSFConsoleWidget::setupCompleter()
{
    m_completerModel = new QStringListModel(this);
    completer = new QCompleter(m_completerModel, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchStartsWith);
    inputLineEdit->setCompleter(completer);

    inputLineEdit->installEventFilter(this);

    connect(inputLineEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        updateCompleterPopup(text);
    });

    connect(completer, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& completion) {
        QString next = m_completionPrefixBase + completion;
        if (!next.endsWith(' ')) {
            next.append(' ');
        }
        inputLineEdit->setText(next);
    });
}

bool MSFConsoleWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == inputLineEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool popupVisible = completer && completer->popup() && completer->popup()->isVisible();

        if (!popupVisible) {
            if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
                const int direction = keyEvent->key() == Qt::Key_Up ? 1 : -1;
                if (m_commandHistory.isEmpty()) {
                    return true;
                }

                if (m_historyIndex == -1) {
                    m_pendingInput = inputLineEdit->text();
                }

                const QString prefix = m_pendingInput.trimmed();
                int index = m_historyIndex;
                for (;;) {
                    index += direction;
                    if (index < 0 || index >= m_commandHistory.size()) {
                        if (direction < 0) {
                            m_historyIndex = -1;
                            inputLineEdit->setText(m_pendingInput);
                        }
                        break;
                    }
                    const QString candidate = m_commandHistory.at(index);
                    if (prefix.isEmpty() || candidate.startsWith(prefix, Qt::CaseInsensitive)) {
                        m_historyIndex = index;
                        inputLineEdit->setText(candidate);
                        break;
                    }
                }
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void MSFConsoleWidget::onConnected()
{
    logMessage("SUCCESS", "Console connected successfully");
    updateRpcConnectionStatus("Connected", "");
    updateConsoleStatus("Ready", "");
    connectWebSocket();
    preheatCompletions();
}

void MSFConsoleWidget::updateMsfServiceStatus(const QString& status, const QString& color)
{
    applyStatusPill(msfServiceStatusLabel, status, color);
    logMessage("STATUS", QString("MSF Service: %1").arg(status));
}

void MSFConsoleWidget::updateRpcConnectionStatus(const QString& status, const QString& color)
{
    applyStatusPill(rpcConnectionStatusLabel, status, color);
    logMessage("STATUS", QString("RPC Connection: %1").arg(status));
}

void MSFConsoleWidget::updateConsoleStatus(const QString& status, const QString& color)
{
    applyStatusPill(consoleStatusValueLabel, status, color);
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
        const QColor color = logColorFor(outputTextEdit, level);
        outputTextEdit->append(QString("<span style='color: %1;'>%2</span>").arg(color.name(QColor::HexRgb), logEntry));
    }
    
    // 输出到调试控制台
    if (kMsvVerboseLogs) {
        qDebug() << "[MSF]" << logEntry;
    }
}

void MSFConsoleWidget::onDisconnected()
{
    logMessage("WARNING", "Console disconnected");
    updateRpcConnectionStatus("Disconnected", "");
    updateConsoleStatus("Not Ready", "");
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
    updateRpcConnectionStatus("WebSocket Connected", "");
    stopOutputPolling();
}

void MSFConsoleWidget::onWsDisconnected()
{
    logMessage("WARNING", "WebSocket disconnected");
    updateRpcConnectionStatus("WebSocket Disconnected", "");
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
        extractOutputCompletions(data);
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
    addHistoryEntry(command);
    addCompletionCandidate(command);

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
        updateMsfServiceStatus("Running", "");
    } else {
        logMessage("ERROR", "Failed to start MSF service");
        if (!response["error"].toString().isEmpty()) {
            logMessage("ERROR", QString("Error: %1").arg(response["error"].toString()));
            outputTextEdit->append("[-] Error: " + response["error"].toString());
        }
        updateMsfServiceStatus("Failed", "");
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

    updateMsfServiceStatus("Stopping...", "");

    QString url = QString("%1/api/msf/controller/stop").arg(m_serverUrl);

    QJsonObject response = HttpReq(url, QByteArray(), m_token);

    if (response["ok"].toBool()) {
        logMessage("SUCCESS", "MSF service stopped successfully");
        outputTextEdit->append("[+] MSF service stopped");
        updateMsfServiceStatus("Stopped", "");
        updateRpcConnectionStatus("Disconnected", "");
        updateConsoleStatus("Not Created", "");
        
        m_currentConsoleId.clear();
    } else {
        logMessage("ERROR", "Failed to stop MSF service");
        outputTextEdit->append("[-] Failed to stop MSF service");
        updateMsfServiceStatus("Failed", "");
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
    
    updateRpcConnectionStatus("Disconnecting...", "");
    
    m_currentConsoleId.clear();
    updateConsoleStatus("Not Created", "");
    
    logMessage("SUCCESS", "RPC disconnected successfully");
    updateRpcConnectionStatus("Disconnected", "");
    
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

    updateRpcConnectionStatus("Connecting...", "");
    
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
        updateRpcConnectionStatus("Connected", "");
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
            updateRpcConnectionStatus("Failed", "");
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

    updateConsoleStatus("Creating...", "");

    QString url = QString("%1/api/msf/console/create").arg(m_serverUrl);

    QJsonObject response = HttpReq(url, QByteArray(), m_token);

    if (response["ok"].toBool()) {
        m_currentConsoleId = response["id"].toString();
        m_lastOutputLength = 0;
        logMessage("SUCCESS", QString("Console created successfully with ID: %1").arg(m_currentConsoleId));
        updateConsoleStatus("Created", "");
        onConnected();
    } else {
        logMessage("ERROR", QString("Failed to create console: %1").arg(response["error"].toString()));
        updateConsoleStatus("Failed", "");
    }
}

void MSFConsoleWidget::updateStatus(const QString& status)
{
    logMessage("DEBUG", QString("Legacy status update: %1").arg(status));
    
    if (status == "running") {
        updateMsfServiceStatus("Running", "");
    } else if (status == "stopped") {
        updateMsfServiceStatus("Stopped", "");
    } else {
        updateMsfServiceStatus(status, "");
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

MSFSessionsWidget::MSFSessionsWidget(const QString& project, QWidget* parent, const bool createDock) : QWidget(parent)
{
    createUI();

    if (createDock) {
        m_dock = new KDDockWidgets::QtWidgets::DockWidget(project + "-MSFSessions", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        m_dock->setWidget(this);
        m_dock->setTitle("MSF Sessions");
    }
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
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    auto* titleLabel = new QLabel("MSF Sessions");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);

    refreshButton = new QPushButton("Refresh");
    refreshButton->setIcon(themedIcon(QIcon::fromTheme("view-refresh"), refreshButton));
    connect(refreshButton, &QPushButton::clicked, this, &MSFSessionsWidget::onRefresh);

    toolbarLayout->addWidget(titleLabel);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    toolbarLayout->addWidget(refreshButton);

    mainLayout->addLayout(toolbarLayout);

    sessionsTable = new QTableWidget();
    sessionsTable->setColumnCount(6);
    sessionsTable->setHorizontalHeaderLabels({"ID", "Type", "Info", "Host", "Arch", ""});
    sessionsTable->horizontalHeader()->setStretchLastSection(true);
    sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sessionsTable->setAlternatingRowColors(true);

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

MSFListenersWidget::MSFListenersWidget(const QString& project, QWidget* parent, const bool createDock) : QWidget(parent)
{
    createUI();

    if (createDock) {
        m_dock = new KDDockWidgets::QtWidgets::DockWidget(project + "-MSFListeners", KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        m_dock->setWidget(this);
        m_dock->setTitle("MSF Listeners");
    }
}

MSFUnifiedWidget::MSFUnifiedWidget(const QString& project, Settings* settings, QWidget* parent)
    : DockTab("MSF", project, ":/icons/terminal")
{
    m_settings = settings;
    m_consoleWidget = new MSFConsoleWidget(project, settings, this, false);
    m_sessionsWidget = new MSFSessionsWidget(project, this, false);
    m_listenersWidget = new MSFListenersWidget(project, this, false);

    createUI();
    dockWidget->setWidget(this);
}

MSFUnifiedWidget::~MSFUnifiedWidget() = default;

void MSFUnifiedWidget::setToken(const QString& token)
{
    if (m_consoleWidget) {
        m_consoleWidget->setToken(token);
    }
    if (m_sessionsWidget) {
        m_sessionsWidget->setToken(token);
    }
    if (m_listenersWidget) {
        m_listenersWidget->setToken(token);
    }
}

void MSFUnifiedWidget::setServerUrl(const QString& url)
{
    if (m_consoleWidget) {
        m_consoleWidget->setServerUrl(url);
    }
    if (m_sessionsWidget) {
        m_sessionsWidget->setServerUrl(url);
    }
    if (m_listenersWidget) {
        m_listenersWidget->setServerUrl(url);
    }
}

void MSFUnifiedWidget::createUI()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(3);

    splitter->addWidget(m_consoleWidget);
    splitter->addWidget(m_sessionsWidget);
    splitter->addWidget(m_listenersWidget);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);

    rootLayout->addWidget(splitter);
}

MSFListenersWidget::~MSFListenersWidget() {}

void MSFListenersWidget::setToken(const QString& token)
{
    m_token = token;
}

void MSFListenersWidget::setServerUrl(const QString& url)
{
    m_serverUrl = url;
}

void MSFListenersWidget::createUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    auto* titleLabel = new QLabel("MSF Listeners");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);

    refreshButton = new QPushButton("Refresh");
    refreshButton->setIcon(themedIcon(QIcon::fromTheme("view-refresh"), refreshButton));
    connect(refreshButton, &QPushButton::clicked, this, &MSFListenersWidget::onRefresh);

    toolbarLayout->addWidget(titleLabel);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    toolbarLayout->addWidget(refreshButton);

    mainLayout->addLayout(toolbarLayout);

    jobsTable = new QTableWidget();
    jobsTable->setColumnCount(4);
    jobsTable->setHorizontalHeaderLabels({"ID", "Name", "Status", ""});
    jobsTable->horizontalHeader()->setStretchLastSection(true);
    jobsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    jobsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    jobsTable->setAlternatingRowColors(true);

    mainLayout->addWidget(jobsTable);
}

void MSFListenersWidget::onJobsUpdate(const QJsonObject& jobs)
{
    m_jobs.clear();
    for (auto it = jobs.begin(); it != jobs.end(); ++it) {
        const QString jobId = it.key();
        const QJsonObject job = it.value().toObject();
        JobItem item;
        item.id = job["id"].toString(jobId);
        item.name = job["name"].toString();
        item.status = job["status"].toString();
        m_jobs[item.id] = item;
    }
    updateTable(jobs);
}

void MSFListenersWidget::onRefresh()
{
    refreshJobs();
}

void MSFListenersWidget::onKill(const QString& jobId)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Kill Listener",
        QString("Kill job %1?").arg(jobId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        HttpReq(
            QString("%1/api/msf/jobs/%2/kill").arg(m_serverUrl).arg(jobId),
            QByteArray(),
            m_token
        );
        refreshJobs();
    }
}

void MSFListenersWidget::updateTable(const QJsonObject& jobs)
{
    Q_UNUSED(jobs)

    jobsTable->setRowCount(m_jobs.count());

    int row = 0;
    for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it) {
        const JobItem& item = it.value();

        jobsTable->setItem(row, 0, new QTableWidgetItem(item.id));
        jobsTable->setItem(row, 1, new QTableWidgetItem(item.name));
        jobsTable->setItem(row, 2, new QTableWidgetItem(item.status));

        QPushButton* killBtn = new QPushButton("Kill");
        killBtn->setStyleSheet("QPushButton { background-color: #141414; border: 1px solid #f44336; color: #f44336; padding: 4px 10px; border-radius: 6px; font-weight: 700; } QPushButton:hover { background-color: #1d1d1d; }");
        connect(killBtn, &QPushButton::clicked, this, [this, item]() { onKill(item.id); });

        QWidget* buttonWidget = new QWidget();
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->addWidget(killBtn);
        buttonLayout->setContentsMargins(0, 0, 0, 0);

        jobsTable->setCellWidget(row, 3, buttonWidget);

        row++;
    }

    jobsTable->resizeColumnsToContents();
}

void MSFListenersWidget::refreshJobs()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty()) return;

    QJsonObject response = HttpReq(
        QString("%1/api/msf/jobs").arg(m_serverUrl),
        QByteArray(),
        m_token
    );

    if (response["ok"].toBool()) {
        onJobsUpdate(response["jobs"].toObject());
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
