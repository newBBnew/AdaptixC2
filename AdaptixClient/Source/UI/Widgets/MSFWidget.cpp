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

MSFConsoleWidget::MSFConsoleWidget(const QString& project, QWidget* parent) : QWidget(parent)
{
    createUI();
    setupCompleter();
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
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(4);

    newConsoleButton = new QPushButton("New Console");
    newConsoleButton->setIcon(QIcon::fromTheme("document-new"));
    connect(newConsoleButton, &QPushButton::clicked, this, &MSFConsoleWidget::refreshConsole);

    clearButton = new QPushButton("Clear");
    clearButton->setIcon(QIcon::fromTheme("edit-clear"));
    connect(clearButton, &QPushButton::clicked, outputTextEdit, &QTextEdit::clear);

    statusLabel = new QLabel("Disconnected");
    statusLabel->setStyleSheet("color: #888;");

    toolbarLayout->addWidget(newConsoleButton);
    toolbarLayout->addWidget(clearButton);
    toolbarLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    toolbarLayout->addWidget(statusLabel);

    mainLayout->addLayout(toolbarLayout);

    outputTextEdit = new QTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setFont(FontManager::getConsoleFont());
    outputTextEdit->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");

    mainLayout->addWidget(outputTextEdit);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(4);

    inputLineEdit = new QLineEdit();
    inputLineEdit->setPlaceholderText("MSF Console (enter MSF commands)");
    inputLineEdit->setFont(FontManager::getConsoleFont());
    inputLineEdit->setStyleSheet("background-color: #2d2d2d; color: #d4d4d4;");

    sendButton = new QPushButton("Send");
    sendButton->setIcon(QIcon::fromTheme("system-run"));

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
    statusLabel->setText("Connected");
    statusLabel->setStyleSheet("color: #4caf50;");
    refreshConsole();
}

void MSFConsoleWidget::onDisconnected()
{
    statusLabel->setText("Disconnected");
    statusLabel->setStyleSheet("color: #f44336;");
    m_currentConsoleId.clear();
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
        QMessageBox::warning(this, "MSF", "Not connected to server");
        return;
    }

    QString command = inputLineEdit->text().trimmed();
    if (command.isEmpty()) return;

    outputTextEdit->append(QString("msf6 > %1").arg(command));
    inputLineEdit->clear();

    if (m_currentConsoleId.isEmpty()) {
        refreshConsole();
    }

    QJsonObject jsonData;
    jsonData["command"] = command;

    QJsonObject response = HttpReq(
        QString("%1/api/msf/console/%2/write").arg(m_serverUrl).arg(m_currentConsoleId),
        QJsonDocument(jsonData).toJson(QJsonDocument::Compact),
        m_token
    ).object();

    if (response["ok"].toBool()) {
        refreshConsole();
    }
}

void MSFConsoleWidget::refreshConsole()
{
    if (m_token.isEmpty() || m_serverUrl.isEmpty()) return;

    QJsonObject response = HttpReq(
        QString("%1/api/msf/console/create").arg(m_serverUrl),
        QByteArray(),
        m_token
    ).object();

    if (response["ok"].toBool()) {
        m_currentConsoleId = response["id"].toString();
        onConnected();
    }
}

MSFSessionsWidget::MSFSessionsWidget(const QString& project, QWidget* parent) : QWidget(parent)
{
    createUI();
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
    ).object();

    if (response["ok"].toBool()) {
        onSessionsUpdate(response["sessions"].toObject());
    }
}
