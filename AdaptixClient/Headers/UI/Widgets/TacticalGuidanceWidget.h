#ifndef ADAPTIXCLIENT_TACTICALGUIDANCEWIDGET_H
#define ADAPTIXCLIENT_TACTICALGUIDANCEWIDGET_H

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

#define MIME_TACTICAL_BLOCK "application/x-adaptix-tactical-block"

class TacticalGuidanceWidget : public DockTab
{
    Q_OBJECT
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    AdaptixWidget* adaptixWidget = nullptr;

    // --- New Tactical Data Structures ---
    struct TacticalVariantData {
        QString id;
        QString name;
        QString commandTemplate;
        int os; // OS_WINDOWS, etc.
        int risk;
        QString opsecNotes;
        QString aiGuidance;
    };

    struct TacticalBlockData {
        QString id;
        QString name;
        QString category;
        QString description;
        QVector<TacticalVariantData> variants;
    };

    struct TacticalNodeInstanceData {
        QString instanceId;
        QString blockId;
        QString variantId;
        QMap<QString, QString> parameters;
        int status; // 0: Pending, 1: Running, 2: Success, 3: Failed
    };

    struct TacticalWorkflowData {
        QString workflowId;
        QString name;
        QVector<TacticalNodeInstanceData> nodes;
        QStringList targetAgents;
    };

    // Column 1: Library
    QWidget* libraryPanel = nullptr;
    QVBoxLayout* libraryLayout = nullptr;
    QLineEdit* librarySearch = nullptr;
    QTreeView* libraryView = nullptr;
    QStandardItemModel* libraryModel = nullptr;
    QSortFilterProxyModel* libraryProxyModel = nullptr;

    // Column 2: Composer
    QWidget* composerPanel = nullptr;
    QVBoxLayout* composerLayout = nullptr;
    QTreeWidget* composerTree = nullptr;
    QWidget* composerActionsRow = nullptr;
    QLineEdit* composerTargetAgents = nullptr;

    QSplitter* mainHSplitter = nullptr;

    // Column 3: Results
    QWidget* resultsPanel = nullptr;
    QVBoxLayout* resultsLayout = nullptr;
    QTreeWidget* resultsTree = nullptr;

    QVector<TacticalWorkflowData> workflows;
    int currentWorkflowIndex = -1;

    QMap<QString, TacticalBlockData> catalogMap; // blockId -> data
    QMap<QString, TacticalVariantData> variantMap; // variantId -> data
    QMap<QString, QTreeWidgetItem*> taskToStepMap; // taskId -> composer step item

    void createLibraryUI();
    void createComposerUI();
    void createResultsUI();
    void initDefaultLibrary();

private Q_SLOTS:
    // Library slots
    void onLibraryBlockSelected();
    void onLibrarySearchChanged(const QString& text);

    // Composer slots
    void onWorkflowSelected();
    void onWorkflowStepClicked(QTreeWidgetItem* item, int column);
    void onRunWorkflowClicked();

    // Results slots
    void onResultsItemClicked(QTreeWidgetItem* item, int column);

public:
    QJsonObject getLibraryAsJson() const;
    QJsonObject getResultsAsJson() const;

    void handleCatalogSync(const QJsonObject& json);
    void handleWorkflowSync(const QJsonObject& json);
    void handleTaskUpdate(const TaskData& task);

    void addStepToWorkflow(const QString& variantId, const QMap<QString, QString>& params);
    void clearWorkflow();
    void executeWorkflow();

private Q_SLOTS:
    void onComposerChanged();
    void onTargetAgentsChanged();
    void syncWorkflowToServer();

private:
    QString renderCommand(const QString& templ, const AgentData& agentData, const QMap<QString, QString>& params) const;
    QString riskLabel(const int risk) const;

public:
    explicit TacticalGuidanceWidget(AdaptixWidget* w);
    ~TacticalGuidanceWidget() override;
};

#endif
