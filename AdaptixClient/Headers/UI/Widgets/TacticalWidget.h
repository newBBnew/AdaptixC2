#ifndef ADAPTIXCLIENT_TACTICALWIDGET_H
#define ADAPTIXCLIENT_TACTICALWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QMap>
#include <QTimer>
#include <QTextEdit>
#include <QSplitter>
#include <QListWidget>
#include <QSpinBox>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

// 命令变体（版本）
struct CommandVariant {
    QString tag;        // 标签: "standard", "obfuscated", "alt1" 等
    QString cmd;        // 实际命令
    QString description; // 变体描述
};

struct TacticalCommand {
    QString id;
    QString name;
    QString cmd;           // 默认命令（向后兼容）
    QString description;
    int status = 0;        // 0=pending, 1=running, 2=done, 3=error
    QString result;
    QList<CommandVariant> variants;  // 命令变体列表
};

struct TacticalGroup {
    QString id;
    QString name;
    QList<TacticalCommand> commands;
};

struct TacticalPhase {
    QString id;
    QString name;
    QString icon;
    QString description;
    QList<TacticalGroup> groups;
};

class TacticalWidget : public DockTab
{
    Q_OBJECT

public:
    explicit TacticalWidget(AdaptixWidget* w);
    
    void setAgent(const QString& agentId);
    KDDockWidgets::QtWidgets::DockWidget* dock() { return this->dockWidget; }

private:
    void createUI();
    void loadPhases();
    void loadWindowsPhases();
    void loadLinuxPhases();
    void updatePhaseTree();
    void executeSelected();
    void executeAll();
    void executeCommand(const QString& phaseId, const QString& groupId, const QString& cmdId);
    void executeVariant(QTreeWidgetItem* cmdItem, int variantIndex);
    void executeFuzzVariants(QTreeWidgetItem* cmdItem);
    void executeDirectCommand(const QString& cmd, const QString& name);
    void showCommandEditor(QTreeWidgetItem* item);  // 统一的命令编辑/预览对话框
    
    AdaptixWidget* adaptixWidget = nullptr;
    
    // UI
    QSplitter* mainSplitter = nullptr;
    QTabWidget* phaseTab = nullptr;
    QMap<QString, QTreeWidget*> phaseTrees;
    QComboBox* agentCombo = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* multiAgentBtn = nullptr;
    QStringList selectedAgentIds;  // 多选的 Agent
    QPushButton* executeSelectedBtn = nullptr;
    QPushButton* nextPhaseBtn = nullptr;
    QPushButton* clearResultBtn = nullptr;
    QPushButton* saveWorkflowBtn = nullptr;
    QPushButton* loadWorkflowBtn = nullptr;
    QLabel* statusLabel = nullptr;
    QProgressBar* progressBar = nullptr;
    QTreeWidget* resultTree = nullptr;  // 改为树形列表，支持展开/折叠
    QLabel* resultTitleLabel = nullptr;
    
    // 任务编排队列 UI（支持多套队列）
    QComboBox* queueSelector = nullptr;        // 队列选择器
    QListWidget* taskQueueList = nullptr;
    QPushButton* addQueueBtn = nullptr;        // 新建队列
    QPushButton* deleteQueueBtn = nullptr;     // 删除队列
    QPushButton* addToQueueBtn = nullptr;
    QPushButton* removeFromQueueBtn = nullptr;
    QPushButton* moveUpBtn = nullptr;
    QPushButton* moveDownBtn = nullptr;
    QPushButton* clearQueueBtn = nullptr;
    QPushButton* runQueueBtn = nullptr;
    QSpinBox* delaySpinBox = nullptr;
    QLabel* queueStatusLabel = nullptr;
    
    // 多队列数据
    QMap<QString, QList<QPair<QString, QString>>> taskQueues;  // 队列名 -> 命令列表
    QString currentQueueName;
    
    // Data
    QList<TacticalPhase> phases;
    QList<TacticalCommand> userCommands;  // 用户自定义命令
    QString currentAgentId;
    QString currentAgentOs;
    int currentPhaseIndex = 0;
    QString userCommandsFile;
    QString commandModsFile;  // 命令修改持久化文件
    QString historyFile;      // 执行历史持久化文件
    QMap<QString, TacticalCommand> commandMods;  // ref -> 修改后的命令
    
    // Execution
    QTimer* executeTimer = nullptr;
    QList<QPair<QString, QString>> pendingCommands; // phase.group.cmd pairs
    QMap<QString, QTreeWidgetItem*> taskToTreeItem;  // taskId -> 结果树项

public Q_SLOTS:
    void onTaskOutput(const QString& agentId, const QString& taskId, int messageType, const QString& output, bool completed);

private Q_SLOTS:
    void onAgentChanged(int index);
    void onRefreshAgents();
    void onExecuteSelected();
    // onExecuteAll 已移除
    void onNextPhase();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void processNextCommand();
    void onTreeContextMenu(const QPoint& pos);
    void onAddCommand();
    void onEditCommand();
    void onDeleteCommand();
    void saveUserCommands();
    void loadUserCommands();
    void saveCommandMods();
    void loadCommandMods();
    void applyCommandMods();
    void onMultiAgentSelect();
    void onSaveWorkflow();
    void onLoadWorkflow();
    void onCommandPreview(QTreeWidgetItem* item, int column);
    
    // 任务编排队列
    void onAddToQueue();
    void onAddToQueueByName(const QString& queueName);  // 添加到指定队列
    void onRemoveFromQueue();
    void onMoveQueueUp();
    void onMoveQueueDown();
    void onClearQueue();
    void onRunQueue();
    void processQueueNext();
    void onAddQueue();           // 新建队列
    void onDeleteQueue();        // 删除队列
    void onQueueChanged(int index);  // 切换队列
    void syncQueueToUI();        // 同步当前队列到 UI
    void syncUIToQueue();        // 同步 UI 到当前队列
    
    // 执行历史
    void saveHistory();
    void loadHistory();
};

#endif // ADAPTIXCLIENT_TACTICALWIDGET_H
