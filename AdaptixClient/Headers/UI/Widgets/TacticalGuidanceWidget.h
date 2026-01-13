#ifndef ADAPTIXCLIENT_TACTICALGUIDANCEWIDGET_H
#define ADAPTIXCLIENT_TACTICALGUIDANCEWIDGET_H

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHash>
#include <QSet>
#include <QTreeView>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;
class QTreeWidgetItem;

#define MIME_TACTICAL_BLOCK "application/x-adaptix-tactical-block"

class TacticalGuidanceWidget : public DockTab
{
    Q_OBJECT
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    AdaptixWidget* adaptixWidget = nullptr;

    // --- New Tactical Data Structures ---
    struct TacticalCommandData {
        QString id;
        QString name;           // Function Description (displayed in tree)
        QString rawCommand;     // Original Command (tooltip)
        QString usage;          // Usage & Explanation
        int os;                 // 1: Win, 2: Linux, 3: Mac
        int arch;               // 0: Any, 1: x86, 2: x64
        int risk;               // 1: Low, 2: Medium, 3: High
        QString description;
    };

    struct TacticalNodeData {
        QString id;
        QString parentId;
        QString name;
        QString type; // "category", "command"
        QString description;
        
        // If command
        TacticalCommandData command;
        
        // Children (for UI construction)
        QVector<TacticalNodeData> children;
    };

    // Per-Agent Execution Queue
    struct AgentExecutionQueue {
        QString agentId;
        QList<QTreeWidgetItem*> commandQueue;  // Commands to execute for this agent
        QTreeWidgetItem* currentCommand = nullptr;  // Currently executing command
        QString currentTaskId;  // Task ID of current command
        bool isWaitingForTask = false;  // Waiting for task completion
        int currentStepIndex = 0;  // Current step in the workflow
    };

    // Column 1: Library
    QWidget* libraryPanel = nullptr;
    QVBoxLayout* libraryLayout = nullptr;
    
    // Agent Selection
    QPushButton* agentSelectBtn = nullptr; // Multi-select dropdown button
    QMenu* agentSelectMenu = nullptr;      // Menu for agents
    QComboBox* agentOsFilter = nullptr;    // Filter library by OS manually or auto

    QLineEdit* librarySearch = nullptr;
    QTreeView* libraryView = nullptr;
    QStandardItemModel* libraryModel = nullptr;
    QSortFilterProxyModel* libraryProxyModel = nullptr;

    // Column 2: Composer (Playbooks)
    QWidget* composerPanel = nullptr;
    QVBoxLayout* composerLayout = nullptr;
    QComboBox* playbookSelector = nullptr; // Select active playbook
    QTreeWidget* composerTree = nullptr; // Root items = Playbooks, Children = Steps
    QWidget* composerActionsRow = nullptr;
    
    // Workflow Settings
    QSpinBox* workflowInterval = nullptr; // Execution interval (seconds)
    QCheckBox* chkStopOnError = nullptr;
    QPushButton* btnAddPlaybook = nullptr;
    QPushButton* btnSaveLocal = nullptr;
    QPushButton* btnPushServer = nullptr; // Sync current state

    QSplitter* mainHSplitter = nullptr;

    // Column 3: Results
    QWidget* resultsPanel = nullptr;
    QVBoxLayout* resultsLayout = nullptr;
    QTreeWidget* resultsTree = nullptr; // Agent -> Step -> Output
    QPushButton* btnClearResults = nullptr;

    QMap<QString, TacticalCommandData> commandMap; // commandId -> data
    QMap<QString, TacticalNodeData> nodeMap;       // nodeId -> data

    bool executionRunning = false;
    QStringList executionTargetAgents;
    QHash<QString, QTreeWidgetItem*> taskIdToComposerItem;
    QHash<QString, QTreeWidgetItem*> resultsAgentItems;

    QJsonArray playbooks;
    QString currentPlaybookId;
    bool composerIsLoading = false;

    // New per-agent execution management
    QHash<QString, AgentExecutionQueue> agentQueues;  // agentId -> execution queue

    void createLibraryUI();
    void createComposerUI();
    void createResultsUI();
    void initDefaultLibrary();
    void refreshAgentList();
    void refreshPlaybookList();
    void buildLibraryTree(QStandardItem* parentItem, const QVector<TacticalNodeData>& nodes);

    // Helper for Add/Edit Command
    bool showCommandDialog(TacticalCommandData& data, QString& parentId, const QString& title);
    
    // Helpers for Management
    void cleanupNodeData(QStandardItem* item);
    QStandardItem* findItemById(const QString& id, QStandardItem* parent = nullptr);
    QMap<QString, QString> getAllCategories(QStandardItem* parent = nullptr, QString prefix = "");

    void saveComposer();
    void loadComposer();
    void rebuildPlaybookList();
    void switchPlaybook(const QString& playbookId);
    void storeCurrentPlaybook();
    void ensureDefaultPlaybook();
    QJsonObject serializeComposerItem(QTreeWidgetItem* item) const;
    QTreeWidgetItem* deserializeComposerItem(const QJsonObject& obj, QTreeWidgetItem* parent = nullptr);

private:
    QJsonObject serializeNode(QStandardItem* item);

private Q_SLOTS:
    // Library slots
    void onLibraryBlockSelected();
    void onLibrarySearchChanged(const QString& text);
    void onLibraryContextMenu(const QPoint& pos);
    void onAgentSelectionChanged();
    void onAgentMenuTriggered(QAction* action);

    // Agent Events
    void onNewAgent(QString agentId);
    void onRemoveAgent(QString agentId);
    void onAgentUpdate(QString agentId);
    void onSynced();

    // Composer slots
    void onComposerContextMenu(const QPoint& pos);
    void onAddPlaybookClicked();
    void onPushWorkflowClicked();
    void onSaveLocalClicked();
    
    // Results slots
    void onResultsItemClicked(QTreeWidgetItem* item, int column);
    void onClearResultsClicked();

public:
    QJsonObject getLibraryAsJson() const;
    QJsonObject getResultsAsJson() const;

    // Local persistence
    void saveLibrary();
    void loadLibrary();
    
    void handleWorkflowSync(const QJsonObject& json);
    void handleTaskUpdate(const TaskData& task);
    void notifyTaskSuccess(const QString& taskId);  // New: Called when UI shows success

    void addStepToActivePlaybook(const QString& commandId, const QMap<QString, QString>& params);
    void clearWorkflow();

    void runActivePlaybook();

    // New per-agent execution methods
    void initializeAgentQueues();
    void executeNextCommandForAgent(const QString& agentId);
    void submitCommandForAgent(const QString& agentId, QTreeWidgetItem* commandItem);

private Q_SLOTS:
    void onComposerChanged();
    void onTargetAgentsChanged();
    void syncWorkflowToServer();
    void onAddGroupClicked();
    void onWorkflowSelected();

private:
    QString riskLabel(const int risk) const;

    QList<QTreeWidgetItem*> collectCommandSteps() const;
    QStringList collectSelectedAgentIds() const;
    void stopExecution();

    // Serialization helpers
    QJsonArray serializeLibraryTree(QStandardItem* parent = nullptr);
    void rebuildLibraryFromJSON(const QJsonArray& nodes);

public:
    explicit TacticalGuidanceWidget(AdaptixWidget* w);
    ~TacticalGuidanceWidget() override;
};

#endif
