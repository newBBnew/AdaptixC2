#include <UI/Widgets/TacticalGuidanceWidget.h>

#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Workers/MCP/MCPBridgeWorker.h>

#include <Client/AuthProfile.h>
#include <Client/Requestor.h>

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <QDrag>
#include <QMimeData>
#include <QUuid>

REGISTER_DOCK_WIDGET(TacticalGuidanceWidget, "Tactical Guidance", true)

TacticalGuidanceWidget::TacticalGuidanceWidget(AdaptixWidget* w)
    : DockTab("Tactical", w->GetProfile()->GetProject(), ":/icons/code_blocks")
{
    adaptixWidget = w;

    mainHSplitter = new QSplitter(Qt::Horizontal, this);
    mainHSplitter->setHandleWidth(3);

    this->createLibraryUI();
    this->createComposerUI();
    this->createResultsUI();
    this->initDefaultLibrary();

    mainHSplitter->setStretchFactor(0, 1);
    mainHSplitter->setStretchFactor(1, 2);
    mainHSplitter->setStretchFactor(2, 2);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(mainHSplitter);

    this->dockWidget->setWidget(this);
}

TacticalGuidanceWidget::~TacticalGuidanceWidget() = default;

void TacticalGuidanceWidget::createLibraryUI()
{
    libraryPanel = new QWidget(this);
    libraryLayout = new QVBoxLayout(libraryPanel);
    libraryLayout->setContentsMargins(4, 4, 4, 4);
    libraryLayout->setSpacing(4);

    librarySearch = new QLineEdit(libraryPanel);
    librarySearch->setPlaceholderText("Search Library...");

    libraryView = new QTreeView(libraryPanel);
    libraryView->setHeaderHidden(true);
    libraryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    libraryView->setDragEnabled(true);
    libraryView->setDragDropMode(QAbstractItemView::DragOnly);
    libraryView->viewport()->installEventFilter(this);

    libraryModel = new QStandardItemModel(this);
    libraryProxyModel = new QSortFilterProxyModel(this);
    libraryProxyModel->setSourceModel(libraryModel);
    libraryProxyModel->setRecursiveFilteringEnabled(true);
    libraryProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    
    libraryView->setModel(libraryProxyModel);

    libraryLayout->addWidget(new QLabel("<b>Command Library</b>"));
    libraryLayout->addWidget(librarySearch);
    libraryLayout->addWidget(libraryView, 1);

    mainHSplitter->addWidget(libraryPanel);

    connect(librarySearch, &QLineEdit::textChanged, this, &TacticalGuidanceWidget::onLibrarySearchChanged);
    connect(libraryView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TacticalGuidanceWidget::onLibraryBlockSelected);
}

void TacticalGuidanceWidget::createComposerUI()
{
    composerPanel = new QWidget(this);
    composerLayout = new QVBoxLayout(composerPanel);
    composerLayout->setContentsMargins(4, 4, 4, 4);
    composerLayout->setSpacing(4);

    composerTree = new QTreeWidget(composerPanel);
    composerTree->setHeaderLabels({"Workflow Steps", "Status"});
    composerTree->setDragEnabled(true);
    composerTree->setAcceptDrops(true);
    composerTree->setDropIndicatorShown(true);
    composerTree->setDragDropMode(QAbstractItemView::DragDrop);
    composerTree->setDefaultDropAction(Qt::MoveAction);
    composerTree->viewport()->installEventFilter(this);

    connect(composerTree->model(), &QAbstractItemModel::rowsInserted, this, &TacticalGuidanceWidget::onComposerChanged);
    connect(composerTree->model(), &QAbstractItemModel::rowsRemoved, this, &TacticalGuidanceWidget::onComposerChanged);
    connect(composerTree->model(), &QAbstractItemModel::modelReset, this, &TacticalGuidanceWidget::onComposerChanged);

    composerActionsRow = new QWidget(composerPanel);
    auto* actionsLayout = new QHBoxLayout(composerActionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);

    auto* btnRun = new QPushButton("Run Workflow", composerActionsRow);
    connect(btnRun, &QPushButton::clicked, this, &TacticalGuidanceWidget::onRunWorkflowClicked);

    auto* btnClear = new QPushButton("Clear", composerActionsRow);
    connect(btnClear, &QPushButton::clicked, this, &TacticalGuidanceWidget::clearWorkflow);

    actionsLayout->addWidget(btnRun);
    actionsLayout->addWidget(btnClear);
    actionsLayout->addStretch();

    composerLayout->addWidget(new QLabel("<b>Workflow Composer</b>"));
    
    composerTargetAgents = new QLineEdit(composerPanel);
    composerTargetAgents->setPlaceholderText("Target Agents (comma separated IDs, e.g. agent1, agent2)");
    composerLayout->addWidget(new QLabel("Target Agents:"));
    composerLayout->addWidget(composerTargetAgents);

    connect(composerTargetAgents, &QLineEdit::textChanged, this, &TacticalGuidanceWidget::onTargetAgentsChanged);

    composerLayout->addWidget(composerTree, 1);
    composerLayout->addWidget(composerActionsRow);

    mainHSplitter->addWidget(composerPanel);

    connect(composerTree, &QTreeWidget::itemClicked, this, &TacticalGuidanceWidget::onWorkflowStepClicked);
}

void TacticalGuidanceWidget::createResultsUI()
{
    resultsPanel = new QWidget(this);
    resultsLayout = new QVBoxLayout(resultsPanel);
    resultsLayout->setContentsMargins(4, 4, 4, 4);
    resultsLayout->setSpacing(4);

    resultsTree = new QTreeWidget(resultsPanel);
    resultsTree->setHeaderLabels({"Agent ID", "Result"});
    resultsTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTree->setAlternatingRowColors(true);

    resultsLayout->addWidget(new QLabel("<b>Results</b>"));
    resultsLayout->addWidget(resultsTree, 1);

    mainHSplitter->addWidget(resultsPanel);

    connect(resultsTree, &QTreeWidget::itemClicked, this, &TacticalGuidanceWidget::onResultsItemClicked);
}

void TacticalGuidanceWidget::initDefaultLibrary()
{
    libraryModel->clear();
    catalogMap.clear();
    variantMap.clear();

    // Helper to add category
    auto addCategory = [&](const QString& name) -> QStandardItem* {
        auto* item = new QStandardItem(name);
        item->setFont(QFont("", -1, QFont::Bold));
        item->setSelectable(false);
        libraryModel->appendRow(item);
        return item;
    };

    // Helper to add block
    auto addBlock = [&](QStandardItem* catItem, const QString& name, const QString& desc) -> QStandardItem* {
        QString id = QUuid::createUuid().toString();
        TacticalBlockData block;
        block.id = id;
        block.name = name;
        block.category = catItem->text();
        block.description = desc;
        
        catalogMap[id] = block;
        
        auto* item = new QStandardItem(name);
        item->setData(id, Qt::UserRole);
        item->setToolTip(desc);
        item->setSelectable(false);
        catItem->appendRow(item);
        
        return item;
    };

    // Helper to add variant
    auto addVariant = [&](QStandardItem* blockItem, const QString& name, const QString& cmd, int os) {
        QString blockId = blockItem->data(Qt::UserRole).toString();
        if (!catalogMap.contains(blockId)) return;

        QString id = QUuid::createUuid().toString();
        TacticalVariantData var;
        var.id = id;
        var.name = name;
        var.commandTemplate = cmd;
        var.os = os;
        
        variantMap[id] = var;
        catalogMap[blockId].variants.push_back(var);
        
        auto* item = new QStandardItem(name);
        item->setData(id, Qt::UserRole);
        if (os == 1) item->setIcon(QIcon(":/icons/os_win_blue"));
        else if (os == 2) item->setIcon(QIcon(":/icons/os_linux_blue"));
        else if (os == 3) item->setIcon(QIcon(":/icons/os_mac_blue"));
        
        blockItem->appendRow(item);
    };

    // --- Reconnaissance ---
    auto* catRecon = addCategory("Reconnaissance");
    
    auto* blockSysInfo = addBlock(catRecon, "System Information", "Gather basic system info");
    addVariant(blockSysInfo, "Basic Info (Win)", "whoami /all && ipconfig /all && systeminfo", 1);
    addVariant(blockSysInfo, "Basic Info (Linux)", "id && ifconfig && uname -a", 2);

    auto* blockProcess = addBlock(catRecon, "Process Discovery", "List running processes");
    addVariant(blockProcess, "Tasklist (Win)", "tasklist /v", 1);
    addVariant(blockProcess, "PS (Linux)", "ps aux", 2);

    // --- Persistence ---
    auto* catPersist = addCategory("Persistence");
    
    auto* blockReg = addBlock(catPersist, "Registry Run Keys", "Add persistence via Registry Run keys");
    addVariant(blockReg, "HKCU Run (Win)", "reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /v Updater /t REG_SZ /d \"{PAYLOAD_PATH}\" /f", 1);

    // --- Credential Access ---
    auto* catCreds = addCategory("Credential Access");
    
    auto* blockLsass = addBlock(catCreds, "LSASS Dump", "Dump LSASS memory for credentials");
    addVariant(blockLsass, "Procdump (Win)", "procdump.exe -ma lsass.exe lsass.dmp", 1);
               
    libraryView->expandAll();
}

QJsonObject TacticalGuidanceWidget::getLibraryAsJson() const
{
    QJsonObject root;
    QJsonArray categories;

    for (int i = 0; i < libraryModel->rowCount(); ++i) {
        QStandardItem* catItem = libraryModel->item(i);
        QJsonObject catObj;
        catObj["name"] = catItem->text();
        
        QJsonArray blocks;
        for (int j = 0; j < catItem->rowCount(); ++j) {
            QStandardItem* blockItem = catItem->child(j);
            QString blockId = blockItem->data(Qt::UserRole).toString();
            if (catalogMap.contains(blockId)) {
                const auto& blockData = catalogMap[blockId];
                QJsonObject blockObj;
                blockObj["id"] = blockData.id;
                blockObj["name"] = blockData.name;
                blockObj["description"] = blockData.description;
                
                QJsonArray variants;
                for (const auto& var : blockData.variants) {
                    QJsonObject varObj;
                    varObj["id"] = var.id;
                    varObj["name"] = var.name;
                    varObj["cmd"] = var.commandTemplate;
                    varObj["os"] = var.os;
                    varObj["risk"] = var.risk;
                    varObj["opsec"] = var.opsecNotes;
                    varObj["ai_guidance"] = var.aiGuidance;
                    variants.append(varObj);
                }
                blockObj["variants"] = variants;
                blocks.append(blockObj);
            }
        }
        catObj["blocks"] = blocks;
        categories.append(catObj);
    }
    
    root["categories"] = categories;
    return root;
}

QJsonObject TacticalGuidanceWidget::getResultsAsJson() const
{
    QJsonObject root;
    QJsonArray results;

    for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* agentItem = resultsTree->topLevelItem(i);
        QString agentId = agentItem->text(0);
        
        for (int j = 0; j < agentItem->childCount(); ++j) {
            QTreeWidgetItem* stepItem = agentItem->child(j);
            QJsonObject resObj;
            results.append(resObj);
        }
    }
    root["results"] = results;
    return root;
}

void TacticalGuidanceWidget::onTargetAgentsChanged()
{
    // No-op for now, or validation logic
}

void TacticalGuidanceWidget::syncWorkflowToServer()
{
    QJsonObject root;
    root["target_agents"] = composerTargetAgents->text();
    
    QJsonArray steps;
    for (int i = 0; i < composerTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = composerTree->topLevelItem(i);
        QJsonObject stepObj;
        stepObj["instance_id"] = item->data(0, Qt::UserRole).toString();
        stepObj["block_id"] = item->data(1, Qt::UserRole).toString();
        stepObj["variant_id"] = item->data(2, Qt::UserRole).toString();
        stepObj["name"] = item->text(0);
        
        QMap<QString, QString> params = item->data(3, Qt::UserRole).value<QMap<QString, QString>>();
        QJsonObject paramsObj;
        for (auto it = params.begin(); it != params.end(); ++it) {
            paramsObj[it.key()] = it.value();
        }
        stepObj["params"] = paramsObj;
        
        steps.append(stepObj);
    }
    root["steps"] = steps;

    // Send to server
    HttpReqTacticalWorkflowUpdateAsync(QJsonDocument(root).toJson(), *adaptixWidget->GetProfile(), [](bool, const QString&, const QJsonObject&){});
}

QString TacticalGuidanceWidget::renderCommand(const QString& templ, const AgentData& agentData, const QMap<QString, QString>& params) const
{
    QString result = templ;
    // Basic replacements
    result.replace("{IP}", agentData.InternalIP);
    result.replace("{USER}", agentData.Username);
    result.replace("{HOST}", agentData.Computer);
    
    // Custom params
    for (auto it = params.begin(); it != params.end(); ++it) {
        result.replace("{" + it.key() + "}", it.value());
    }
    return result;
}

void TacticalGuidanceWidget::onLibrarySearchChanged(const QString& text)
{
    if (libraryProxyModel)
        libraryProxyModel->setFilterWildcard(text);
}

void TacticalGuidanceWidget::onWorkflowSelected()
{
    // Placeholder
}

void TacticalGuidanceWidget::onResultsItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    // Placeholder
}

void TacticalGuidanceWidget::addStepToWorkflow(const QString& variantId, const QMap<QString, QString>& params)
{
    if (!variantMap.contains(variantId)) return;
    const auto& variant = variantMap[variantId];
    
    auto* item = new QTreeWidgetItem(composerTree);
    item->setText(0, variant.name);
    item->setText(1, "Pending");
    item->setData(0, Qt::UserRole, QUuid::createUuid().toString());
    item->setData(1, Qt::UserRole, ""); // Block ID unknown here, optional
    item->setData(2, Qt::UserRole, variantId);
    item->setData(3, Qt::UserRole, QVariant::fromValue(params));
    
    if (variant.os == 1) item->setIcon(0, QIcon(":/icons/os_win_blue"));
    else if (variant.os == 2) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
    else if (variant.os == 3) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
    
    syncWorkflowToServer();
}

void TacticalGuidanceWidget::clearWorkflow()
{
    composerTree->clear();
    syncWorkflowToServer();
}

void TacticalGuidanceWidget::executeWorkflow()
{
    onRunWorkflowClicked();
}

void TacticalGuidanceWidget::onWorkflowStepClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item) return;

    // Logic removed as UI column is gone
}

void TacticalGuidanceWidget::handleCatalogSync(const QJsonObject& json)
{
    if (json["action"].toString() != "sync_all") return;

    libraryModel->clear();
    catalogMap.clear();
    variantMap.clear();

    QJsonArray categories = json["categories"].toArray();

    for (const QJsonValue& catVal : categories) {
        QJsonObject catObj = catVal.toObject();
        auto* catItem = new QStandardItem(catObj["name"].toString());
        catItem->setFont(QFont("", -1, QFont::Bold));
        catItem->setSelectable(false);

        QJsonArray blocks = catObj["blocks"].toArray();
        for (const QJsonValue& blockVal : blocks) {
            QJsonObject blockObj = blockVal.toObject();
            
            TacticalBlockData block;
            block.id = blockObj["id"].toString();
            block.name = blockObj["name"].toString();
            block.category = catObj["name"].toString();
            block.description = blockObj["description"].toString();

            auto* blockItem = new QStandardItem(block.name);
            blockItem->setData(block.id, Qt::UserRole);
            blockItem->setToolTip(block.description);
            blockItem->setSelectable(false);

            QJsonArray variants = blockObj["variants"].toArray();
            for (const QJsonValue& varVal : variants) {
                QJsonObject varObj = varVal.toObject();
                
                TacticalVariantData variant;
                variant.id = varObj["id"].toString();
                variant.name = varObj["name"].toString();
                variant.commandTemplate = varObj["cmd"].toString();
                variant.os = varObj["os"].toInt();
                variant.risk = varObj["risk"].toInt();
                variant.opsecNotes = varObj["opsec"].toString();
                variant.aiGuidance = varObj["ai_guidance"].toString();

                block.variants.push_back(variant);
                variantMap[variant.id] = variant;

                auto* varItem = new QStandardItem(variant.name);
                varItem->setData(variant.id, Qt::UserRole);
                
                // Set icon based on OS
                if (variant.os == 1) varItem->setIcon(QIcon(":/icons/os_win_blue"));
                else if (variant.os == 2) varItem->setIcon(QIcon(":/icons/os_linux_blue"));
                else if (variant.os == 3) varItem->setIcon(QIcon(":/icons/os_mac_blue"));

                blockItem->appendRow(varItem);
            }
            catItem->appendRow(blockItem);
            catalogMap[block.id] = block;
        }
        libraryModel->appendRow(catItem);
    }
    libraryView->expandAll();
}

void TacticalGuidanceWidget::handleWorkflowSync(const QJsonObject& json)
{
    QString action = json["action"].toString();
    if (action == "clear") {
        QSignalBlocker blocker(composerTree);
        composerTree->clear();
        taskToStepMap.clear();
        return;
    }

    if (action == "update") {
        QSignalBlocker blocker(composerTree);
        composerTree->clear();
        taskToStepMap.clear();

        QJsonArray steps = json["steps"].toArray();
        for (const QJsonValue& stepVal : steps) {
            QJsonObject stepObj = stepVal.toObject();
            QString instanceId = stepObj["instance_id"].toString();
            QString blockId = stepObj["block_id"].toString();
            QString variantId = stepObj["variant_id"].toString();
            QString name = stepObj["name"].toString();

            auto* item = new QTreeWidgetItem(composerTree);
            item->setText(0, name);
            item->setText(1, "Pending");
            item->setData(0, Qt::UserRole, instanceId);
            item->setData(1, Qt::UserRole, blockId);
            item->setData(2, Qt::UserRole, variantId);
            
            // Restore params
            QJsonObject paramsObj = stepObj["params"].toObject();
            QMap<QString, QString> params;
            for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
                params[it.key()] = it.value().toString();
            }
            item->setData(3, Qt::UserRole, QVariant::fromValue(params));

            if (variantMap.contains(variantId)) {
                int os = variantMap[variantId].os;
                if (os == OS_WINDOWS) item->setIcon(0, QIcon(":/icons/os_win_blue"));
                else if (os == OS_LINUX) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
                else if (os == OS_MAC) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
            }
        }

        QString targets = json["target_agents"].toString();
        composerTargetAgents->setText(targets);
    }
}

bool TacticalGuidanceWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == libraryView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                auto index = libraryView->indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    auto sourceIndex = libraryProxyModel->mapToSource(index);
                    auto* item = libraryModel->itemFromIndex(sourceIndex);
                    if (item && item->rowCount() == 0 && item->parent() && item->parent()->parent()) {
                        // Start drag if it's a variant (depth 2: category -> block -> variant)
                        QDrag* drag = new QDrag(this);
                        QMimeData* mimeData = new QMimeData;
                        
                        QString variantId = item->data(Qt::UserRole).toString();
                        mimeData->setData(MIME_TACTICAL_BLOCK, variantId.toUtf8());
                        
                        drag->setMimeData(mimeData);
                        drag->exec(Qt::CopyAction);
                        return true;
                    }
                }
            }
        }
    } else if (obj == composerTree->viewport()) {
        if (event->type() == QEvent::DragEnter) {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasFormat(MIME_TACTICAL_BLOCK)) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragMove) {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (dragEvent->mimeData()->hasFormat(MIME_TACTICAL_BLOCK)) {
                dragEvent->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            if (dropEvent->mimeData()->hasFormat(MIME_TACTICAL_BLOCK)) {
                QByteArray data = dropEvent->mimeData()->data(MIME_TACTICAL_BLOCK);
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString blockId = obj["block_id"].toString();
                    QString variantId = obj["variant_id"].toString();
                    QString name = obj["name"].toString();

                    // Create node instance
                    auto* item = new QTreeWidgetItem(composerTree);
                    item->setText(0, name);
                    item->setText(1, "Pending");
                    item->setData(0, Qt::UserRole, QUuid::createUuid().toString()); // instanceId
                    item->setData(1, Qt::UserRole, blockId);
                    item->setData(2, Qt::UserRole, variantId);
                    
                    // Set icon based on variant OS if available
                    if (variantMap.contains(variantId)) {
                        int os = variantMap[variantId].os;
                        if (os == OS_WINDOWS) item->setIcon(0, QIcon(":/icons/os_win_blue"));
                        else if (os == OS_LINUX) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
                        else if (os == OS_MAC) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
                    }

                    dropEvent->acceptProposedAction();
                    return true;
                }
            }
        }
    }

    return DockTab::eventFilter(obj, event);
}

void TacticalGuidanceWidget::onRunWorkflowClicked()
{
    QString targetsRaw = composerTargetAgents->text().trimmed();
    if (targetsRaw.isEmpty()) {
        QMessageBox::warning(this, "Tactical", "Please specify at least one target Agent ID.");
        return;
    }

    QStringList targetAgents = targetsRaw.split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < composerTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = composerTree->topLevelItem(i);
        QString variantId = item->data(2, Qt::UserRole).toString();
        
        if (!variantMap.contains(variantId)) continue;
        const auto& variant = variantMap[variantId];

        for (const QString& agentId : targetAgents) {
            QString agentName = agentId;
            if (adaptixWidget->AgentsMap.contains(agentId)) {
                agentName = adaptixWidget->AgentsMap[agentId]->data.Name;
            }

            // Prepare command data
            QJsonObject dataJson;
            dataJson["name"] = agentName;
            dataJson["id"] = agentId;
            dataJson["ui"] = true;
            
            // Perform variable replacement (Agent + Custom Params)
            QString finalCommand = variant.commandTemplate;
            QMap<QString, QString> stepParams = item->data(3, Qt::UserRole).value<QMap<QString, QString>>();
            if (adaptixWidget->AgentsMap.contains(agentId)) {
                finalCommand = renderCommand(variant.commandTemplate, adaptixWidget->AgentsMap[agentId]->data, stepParams);
            }
            dataJson["cmdline"] = finalCommand;
            
            dataJson["data"] = ""; // Simple commands don't need structured data yet
            dataJson["ax_hook_id"] = "";
            dataJson["ax_handler_id"] = "";
            
            QByteArray jsonData = QJsonDocument(dataJson).toJson();
            
            HttpReqAgentCommandAsync(jsonData, *adaptixWidget->GetProfile(), [this, item, agentId](bool success, const QString &message, const QJsonObject&) {
                if (success) {
                    QString taskId = message;
                    taskToStepMap[taskId] = item;

                    QTreeWidgetItem* agentItem = nullptr;
                    for (int j = 0; j < resultsTree->topLevelItemCount(); ++j) {
                        if (resultsTree->topLevelItem(j)->text(0) == agentId) {
                            agentItem = resultsTree->topLevelItem(j);
                            break;
                        }
                    }
                    if (!agentItem) {
                        agentItem = new QTreeWidgetItem(resultsTree);
                        agentItem->setText(0, agentId);
                        agentItem->setFont(0, QFont("", -1, QFont::Bold));
                    }

                    auto* resultStepItem = new QTreeWidgetItem(agentItem);
                    resultStepItem->setText(0, item->text(0)); 
                    resultStepItem->setData(0, Qt::UserRole, taskId);
                    resultStepItem->setText(1, "Running...");
                } else {
                    item->setText(1, "Failed");
                }
            });
        }
    }
}

void TacticalGuidanceWidget::onComposerChanged()
{
    // 可以在这里处理工作流变更的逻辑，例如保存到服务器
    this->syncWorkflowToServer();
}

void TacticalGuidanceWidget::onLibraryBlockSelected()
{
    auto index = libraryView->currentIndex();
    if (!index.isValid()) return;

    // Map through proxy model
    auto sourceIndex = libraryProxyModel->mapToSource(index);
    auto* item = libraryModel->itemFromIndex(sourceIndex);
    if (!item) return;

    QString variantId = item->data(Qt::UserRole).toString();
    if (variantId.isEmpty() || !variantMap.contains(variantId)) {
        return;
    }
}

void TacticalGuidanceWidget::handleTaskUpdate(const TaskData& task)
{
    // Check if this task corresponds to a step in our workflow
    if (taskToStepMap.contains(task.TaskId)) {
        QTreeWidgetItem* item = taskToStepMap[task.TaskId];
        if (item) {
            if (task.Completed) {
                if (task.Status == "Success") {
                    item->setText(1, "Success");
                    item->setIcon(1, QIcon(":/icons/success")); // Assuming icon exists, or just text
                } else {
                    item->setText(1, "Failed");
                    item->setIcon(1, QIcon(":/icons/error"));
                }
            } else {
                item->setText(1, "Running");
            }
        }
    }
}
