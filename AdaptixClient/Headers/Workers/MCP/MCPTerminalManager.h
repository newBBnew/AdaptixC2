#ifndef MCPTERMINALMANAGER_H
#define MCPTERMINALMANAGER_H

#include <main.h>
#include <QObject>
#include <QMap>
#include <QByteArray>
#include <QThread>
#include <Workers/TerminalWorker.h>

class MCPTerminalManager : public QObject
{
    Q_OBJECT

    struct TerminalSession {
        TerminalWorker* worker;
        QThread* thread;
        QByteArray outputBuffer;
        QString agentId;
        QString terminalId;
    };

    QMap<QString, TerminalSession*> sessions;
    static MCPTerminalManager* m_instance;

public:
    explicit MCPTerminalManager(QObject* parent = nullptr);
    ~MCPTerminalManager();

    static MCPTerminalManager* instance();

    QString openSession(const QString& agentId, const QString& program, int rows, int cols, const QString& token, const QUrl& wsUrl, int oemCP);
    bool writeSession(const QString& terminalId, const QByteArray& data);
    QByteArray readSession(const QString& terminalId, bool clear = true);
    void closeSession(const QString& terminalId);
    QList<QString> getSessionIds() const;
    QString cleanAnsi(const QByteArray& data);

private:
};

#endif
