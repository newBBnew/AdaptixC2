#ifndef ADAPTIXCLIENT_MSFCONSOLEWIDGET_H
#define ADAPTIXCLIENT_MSFCONSOLEWIDGET_H

#include <main.h>

#include <kddockwidgets/qtwidgets/views/DockWidget.h>

class MSFConsoleWidget : public QWidget
{
Q_OBJECT
public:
    explicit MSFConsoleWidget(const QString& project, QWidget* parent = nullptr);
    ~MSFConsoleWidget() override;

    void setToken(const QString& token);
    void setServerUrl(const QString& url);

    KDDockWidgets::QtWidgets::DockWidget* dock() const { return m_dock; }

Q_SIGNALS:
    void consoleOutput(const QString& consoleId, const QString& data, bool busy);

public Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onConsoleOutput(const QString& consoleId, const QString& data, bool busy);
    void sendCommand();

private:
    void createUI();
    void setupCompleter();
    void refreshConsole();

    QString m_token;
    QString m_serverUrl;
    QString m_currentConsoleId;

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;

    QVBoxLayout* mainLayout = nullptr;
    QHBoxLayout* toolbarLayout = nullptr;

    QTextEdit* outputTextEdit = nullptr;
    QLineEdit* inputLineEdit = nullptr;
    QPushButton* sendButton = nullptr;
    QPushButton* clearButton = nullptr;
    QPushButton* newConsoleButton = nullptr;
    QLabel* statusLabel = nullptr;

    QCompleter* completer = nullptr;
    QStringList msfCommands;

    static const QStringList MSF_COMPLETER_COMMANDS;
};

class MSFSessionsWidget : public QWidget
{
Q_OBJECT
public:
    explicit MSFSessionsWidget(const QString& project, QWidget* parent = nullptr);
    ~MSFSessionsWidget() override;

    void setToken(const QString& token);
    void setServerUrl(const QString& url);
    void refreshSessions();

    KDDockWidgets::QtWidgets::DockWidget* dock() const { return m_dock; }

public Q_SLOTS:
    void onSessionNew(const QJsonObject& session);
    void onSessionClosed(const QString& sessionId);
    void onSessionsUpdate(const QJsonObject& sessions);
    void onRefresh();
    void onInteract(const QString& sessionId);
    void onKill(const QString& sessionId);

private:
    void createUI();
    void updateTable(const QJsonObject& sessions);

    QString m_token;
    QString m_serverUrl;

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;

    QVBoxLayout* mainLayout = nullptr;
    QHBoxLayout* toolbarLayout = nullptr;

    QTableWidget* sessionsTable = nullptr;
    QPushButton* refreshButton = nullptr;

    struct SessionItem {
        QString id;
        QString type;
        QString info;
        QString host;
    };
    QMap<QString, SessionItem> m_sessions;
};

#endif
