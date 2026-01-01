#include <Workers/MCP/MCPTerminalManager.h>
#include <QRegularExpression>
#include <QDebug>

MCPTerminalManager* MCPTerminalManager::m_instance = nullptr;

MCPTerminalManager::MCPTerminalManager(QObject* parent) : QObject(parent)
{
    m_instance = this;
}

MCPTerminalManager::~MCPTerminalManager()
{
    for (auto* session : sessions.values()) {
        closeSession(session->terminalId);
    }
}

MCPTerminalManager* MCPTerminalManager::instance()
{
    return m_instance;
}

QString MCPTerminalManager::openSession(const QString& agentId, const QString& program, int rows, int cols, const QString& token, const QUrl& wsUrl, int oemCP)
{
    QString terminalId = GenerateRandomString(8, "hex");
    QString base64Program = program.toUtf8().toBase64();
    
    // agentId|terminalId|program|sizeH|sizeW|OecmCP
    QString terminalData = QString("%1|%2|%3|%4|%5|%6")
        .arg(agentId)
        .arg(terminalId)
        .arg(base64Program)
        .arg(rows > 0 ? rows : 24)
        .arg(cols > 0 ? cols : 80)
        .arg(oemCP)
        .toUtf8().toBase64();

    auto* session = new TerminalSession();
    session->agentId = agentId;
    session->terminalId = terminalId;
    session->thread = new QThread();
    session->worker = new TerminalWorker(nullptr, token, wsUrl, terminalData);
    session->worker->moveToThread(session->thread);

    connect(session->thread, &QThread::started, session->worker, &TerminalWorker::start);
    connect(session->worker, &TerminalWorker::finished, session->thread, &QThread::quit);
    connect(session->worker, &TerminalWorker::binaryMessageToTerminal, this, [this, terminalId](const QByteArray& msg) {
        if (sessions.contains(terminalId)) {
            sessions[terminalId]->outputBuffer.append(msg);
        }
    });

    sessions[terminalId] = session;
    session->thread->start();

    return terminalId;
}

bool MCPTerminalManager::writeSession(const QString& terminalId, const QByteArray& data)
{
    if (!sessions.contains(terminalId)) return false;
    auto* session = sessions[terminalId];
    QMetaObject::invokeMethod(session->worker, "sendData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    return true;
}

QByteArray MCPTerminalManager::readSession(const QString& terminalId, bool clear)
{
    if (!sessions.contains(terminalId)) return QByteArray();
    auto* session = sessions[terminalId];
    QByteArray output = session->outputBuffer;
    if (clear) {
        session->outputBuffer.clear();
    }
    return output;
}

void MCPTerminalManager::closeSession(const QString& terminalId)
{
    if (!sessions.contains(terminalId)) return;
    auto* session = sessions[terminalId];
    
    QMetaObject::invokeMethod(session->worker, "stop", Qt::QueuedConnection);
    session->thread->quit();
    session->thread->wait();
    
    delete session->worker;
    delete session->thread;
    delete session;
    
    sessions.remove(terminalId);
}

QList<QString> MCPTerminalManager::getSessionIds() const
{
    return sessions.keys();
}

QString MCPTerminalManager::cleanAnsi(const QByteArray& data)
{
    QString text = QString::fromUtf8(data);
    // Regex to match ANSI escape sequences
    static QRegularExpression ansiRegex("\x1B\\[[0-9;]*[a-zA-Z]");
    text.remove(ansiRegex);
    return text;
}

