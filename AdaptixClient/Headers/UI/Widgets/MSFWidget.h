#ifndef ADAPTIXCLIENT_MSFCONSOLEWIDGET_H
#define ADAPTIXCLIENT_MSFCONSOLEWIDGET_H

#include <main.h>
#include <Client/Settings.h>

#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <UI/Widgets/AbstractDock.h>
#include <QtWebSockets/QWebSocket>
#include <QStringListModel>
#include <QSet>

class MSFConsoleWidget : public QWidget
{
Q_OBJECT
public:
    explicit MSFConsoleWidget(const QString& project, Settings* settings = nullptr, QWidget* parent = nullptr, bool createDock = true);
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
    void onWsConnected();
    void onWsDisconnected();
    void onWsMessage(const QByteArray& message);
    void onStartMsf();
    void onStopMsf();
    void onConnectRpc();
    void onDisconnectRpc();
    void onMsfrpcdStatus(const QJsonObject& response);
    void checkMsfrpcdStatus();
    void connectMsfApi();
    void connectMsfApiWithRetry();
    void fetchConsoleOutput();

private:
    bool eventFilter(QObject* obj, QEvent* event) override;

    void createUI();
    void setupCompleter();
    void refreshConsole();
    void connectWebSocket();
    void disconnectWebSocket();
    void updateStatus(const QString& status);
    void updateMsfServiceStatus(const QString& status, const QString& color = "#888");
    void updateRpcConnectionStatus(const QString& status, const QString& color = "#888");
    void updateConsoleStatus(const QString& status, const QString& color = "#888");
    void logMessage(const QString& level, const QString& message);
    void startOutputPolling();
    void stopOutputPolling();
    void updateCompleterModel();
    void updateCompleterPopup(const QString& text);
    void addCompletionCandidate(const QString& text);
    void addHistoryEntry(const QString& command);
    void extractOutputCompletions(const QString& output);
    void preheatCompletions();

    QString m_token;
    QString m_serverUrl;
    QString m_currentConsoleId;
    Settings* m_settings = nullptr;

    // MSF 配置
    QString m_msfHost;
    int m_msfPort;
    QString m_msfUser;
    QString m_msfPassword;
    bool m_msfSSL;

    // Output polling
    QTimer* m_outputPollingTimer = nullptr;
    int m_lastOutputLength = 0;

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;

    QVBoxLayout* mainLayout = nullptr;
    QHBoxLayout* statusLayout = nullptr;
    QHBoxLayout* toolbarLayout = nullptr;

    // Status display components
    QLabel* msfServiceLabel = nullptr;
    QLabel* msfServiceStatusLabel = nullptr;
    QLabel* rpcConnectionLabel = nullptr;
    QLabel* rpcConnectionStatusLabel = nullptr;
    QLabel* consoleStatusLabel = nullptr;
    QLabel* consoleStatusValueLabel = nullptr;
    
    // Control buttons
    QPushButton* startMsfButton = nullptr;
    QPushButton* stopMsfButton = nullptr;
    QPushButton* connectRpcButton = nullptr;
    QPushButton* disconnectRpcButton = nullptr;
    QPushButton* newConsoleButton = nullptr;
    QPushButton* clearButton = nullptr;

    QTextEdit* outputTextEdit = nullptr;
    QLineEdit* inputLineEdit = nullptr;
    QPushButton* sendButton = nullptr;

    QCompleter* completer = nullptr;
    QStringListModel* m_completerModel = nullptr;
    QSet<QString> m_completionCache;
    QStringList m_commandHistory;
    int m_historyIndex = -1;
    QString m_pendingInput;
    QString m_completionPrefixBase;
    bool m_preheatDone = false;
    int m_preheatAttempts = 0;

    QWebSocket* m_webSocket = nullptr;

};

class MSFSessionsWidget : public QWidget
{
Q_OBJECT
public:
    explicit MSFSessionsWidget(const QString& project, QWidget* parent = nullptr, bool createDock = true);
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

class MSFListenersWidget : public QWidget
{
Q_OBJECT
public:
    explicit MSFListenersWidget(const QString& project, QWidget* parent = nullptr, bool createDock = true);
    ~MSFListenersWidget() override;

    void setToken(const QString& token);
    void setServerUrl(const QString& url);
    void refreshJobs();

    KDDockWidgets::QtWidgets::DockWidget* dock() const { return m_dock; }

public Q_SLOTS:
    void onJobsUpdate(const QJsonObject& jobs);
    void onRefresh();
    void onKill(const QString& jobId);

private:
    void createUI();
    void updateTable(const QJsonObject& jobs);

    QString m_token;
    QString m_serverUrl;

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;

    QVBoxLayout* mainLayout = nullptr;
    QHBoxLayout* toolbarLayout = nullptr;

    QTableWidget* jobsTable = nullptr;
    QPushButton* refreshButton = nullptr;

    struct JobItem {
        QString id;
        QString name;
        QString status;
    };
    QMap<QString, JobItem> m_jobs;
};

class MSFUnifiedWidget : public DockTab
{
    Q_OBJECT
public:
    explicit MSFUnifiedWidget(const QString& project, Settings* settings = nullptr, QWidget* parent = nullptr);
    ~MSFUnifiedWidget() override;

    void setToken(const QString& token);
    void setServerUrl(const QString& url);

private:
    void createUI();

    Settings* m_settings = nullptr;
    MSFConsoleWidget* m_consoleWidget = nullptr;
    MSFSessionsWidget* m_sessionsWidget = nullptr;
    MSFListenersWidget* m_listenersWidget = nullptr;
};

#endif
