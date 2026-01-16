#include <UI/Widgets/TacticalGuidanceWidget.h>

#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Workers/MCP/MCPBridgeWorker.h>

#include <Client/AuthProfile.h>
#include <Client/CommandSubmitter.h>
#include <Client/Requestor.h>

#include <Agent/Commander.h>
#include <Utils/Convert.h>

#include <functional>

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSignalBlocker>
#include <QTimer>
#include <QMouseEvent>
#include <QUrl>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QDir>

#include <QHeaderView>

#include <QDrag>
#include <QMimeData>
#include <QUuid>

#include <QLineEdit>
#include <QStyledItemDelegate>

#include <UI/Widgets/TasksWidget.h>

namespace {
    const bool kTacticalVerboseLogs = false;
#define TG_DEBUG if (kTacticalVerboseLogs) qDebug()
    constexpr int ROLE_NODE_ID  = Qt::UserRole;
    constexpr int ROLE_NODE_TYPE = Qt::UserRole + 1;
    constexpr int ROLE_COMMAND_OS = Qt::UserRole + 2;

    class StickyCheckMenu final : public QMenu {
    public:
        explicit StickyCheckMenu(QWidget* parent = nullptr)
            : QMenu(parent)
        {
            closeTimer.setSingleShot(true);
            closeTimer.setInterval(350);
            connect(&closeTimer, &QTimer::timeout, this, &QMenu::close);
        }

    protected:
        void mouseReleaseEvent(QMouseEvent* e) override
        {
            QAction* act = actionAt(e->pos());
            if (act && act->isEnabled() && act->isCheckable()) {
                act->trigger();
                setActiveAction(act);
                e->accept();
                return;
            }
            QMenu::mouseReleaseEvent(e);
        }

        void enterEvent(QEnterEvent* e) override
        {
            closeTimer.stop();
            QMenu::enterEvent(e);
        }

        void leaveEvent(QEvent* e) override
        {
            closeTimer.start();
            QMenu::leaveEvent(e);
        }

    private:
        QTimer closeTimer;
    };

    class TacticalLibraryProxyModel final : public QSortFilterProxyModel {
    public:
        explicit TacticalLibraryProxyModel(QObject* parent = nullptr)
            : QSortFilterProxyModel(parent) {}

        void setOsFilter(const int os)
        {
            if (osFilter == os)
                return;
            osFilter = os;
            invalidateFilter();
        }

    protected:
        bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
        {
            const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
            if (!idx.isValid())
                return false;

            const QString type = idx.data(ROLE_NODE_TYPE).toString();

            if (type == "command") {
                if (osFilter != 0) {
                    const int os = idx.data(ROLE_COMMAND_OS).toInt();
                    if (os != 0 && os != osFilter)
                        return false;
                }
                return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
            }

            // category: accept if itself matches text OR any child matches (os+text)
            if (QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
                return true;

            const int childCount = sourceModel()->rowCount(idx);
            for (int i = 0; i < childCount; ++i) {
                if (filterAcceptsRow(i, idx))
                    return true;
            }
            return false;
        }

    private:
        int osFilter = 0; // 0=All, 1=Win,2=Linux,3=mac
    };

    class RawCommandEditorDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
            if (lineEdit) {
                lineEdit->setFrame(false);
                lineEdit->setContentsMargins(0, 0, 0, 0);
                lineEdit->setTextMargins(0, 0, 0, 0);
                lineEdit->setAutoFillBackground(true);
                lineEdit->setAttribute(Qt::WA_OpaquePaintEvent, true);

                lineEdit->setStyleSheet(
                    "QLineEdit {"
                    "  background-color: palette(base);"
                    "  color: palette(text);"
                    "  selection-background-color: palette(highlight);"
                    "  selection-color: palette(highlighted-text);"
                    " }"
                );

                // Ensure editor colors follow the view palette (avoid transparent editor under some themes)
                QPalette pal = lineEdit->palette();
                pal.setColor(QPalette::Base, parent->palette().color(QPalette::Base));
                pal.setColor(QPalette::Text, parent->palette().color(QPalette::Text));
                pal.setColor(QPalette::Highlight, parent->palette().color(QPalette::Highlight));
                pal.setColor(QPalette::HighlightedText, parent->palette().color(QPalette::HighlightedText));
                lineEdit->setPalette(pal);
            }
            return editor;
        }

        void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            Q_UNUSED(index);
            if (editor)
                editor->setGeometry(option.rect);
        }
    };
}

REGISTER_DOCK_WIDGET(TacticalGuidanceWidget, "Tactical Guidance", true)

TacticalGuidanceWidget::TacticalGuidanceWidget(AdaptixWidget* w)
    : DockTab("Tactical", w->GetProfile()->GetProject(), ":/icons/code_blocks")
{
    adaptixWidget = w;

    mainHSplitter = new QSplitter(Qt::Horizontal, this);
    mainHSplitter->setHandleWidth(3);

    layoutSaveTimer = new QTimer(this);
    layoutSaveTimer->setSingleShot(true);
    layoutSaveTimer->setInterval(450);
    connect(layoutSaveTimer, &QTimer::timeout, this, &TacticalGuidanceWidget::saveLayout);

    connect(adaptixWidget, &AdaptixWidget::eventNewAgent, this, &TacticalGuidanceWidget::onNewAgent);
    connect(adaptixWidget, &AdaptixWidget::eventRemoveAgent, this, &TacticalGuidanceWidget::onRemoveAgent);
    connect(adaptixWidget, &AdaptixWidget::eventAgentUpdate, this, &TacticalGuidanceWidget::onAgentUpdate);
    connect(adaptixWidget, &AdaptixWidget::SyncedSignal, this, &TacticalGuidanceWidget::onSynced);

    this->createLibraryUI();
    this->createComposerUI();
    this->createResultsUI();
    
    // Try to load local library, if empty/fail, init default
    this->loadLibrary();
    if (libraryModel->rowCount() == 0) {
        this->initDefaultLibrary();
        this->saveLibrary();
    }

    this->loadComposer();

    connect(mainHSplitter, &QSplitter::splitterMoved, this, &TacticalGuidanceWidget::scheduleSaveLayout);
    if (composerTree && composerTree->header()) {
        connect(composerTree->header(), &QHeaderView::sectionResized, this, &TacticalGuidanceWidget::scheduleSaveLayout);
        connect(composerTree->header(), &QHeaderView::sectionMoved, this, &TacticalGuidanceWidget::scheduleSaveLayout);
    }
    if (resultsTree && resultsTree->header()) {
        connect(resultsTree->header(), &QHeaderView::sectionResized, this, &TacticalGuidanceWidget::scheduleSaveLayout);
        connect(resultsTree->header(), &QHeaderView::sectionMoved, this, &TacticalGuidanceWidget::scheduleSaveLayout);
    }

    mainHSplitter->setStretchFactor(0, 2);
    mainHSplitter->setStretchFactor(1, 2);
    mainHSplitter->setStretchFactor(2, 1);

    QTimer::singleShot(0, this, [this]() {
        if (!mainHSplitter)
            return;
        if (loadLayout())
            return;

        int w = this->width();
        if (w <= 0)
            w = 1200;
        int w0 = qMax(300, static_cast<int>(w * 0.28));
        int w1 = qMax(420, static_cast<int>(w * 0.44));
        int w2 = qMax(320, w - w0 - w1);
        mainHSplitter->setSizes({w0, w1, w2});
    });

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(mainHSplitter);

    this->dockWidget->setWidget(this);
}

TacticalGuidanceWidget::~TacticalGuidanceWidget() = default;

void TacticalGuidanceWidget::onNewAgent(QString agentId)
{
    refreshAgentList();
}

void TacticalGuidanceWidget::onRemoveAgent(QString agentId)
{
    refreshAgentList();
}

void TacticalGuidanceWidget::onAgentUpdate(QString agentId)
{
    refreshAgentList();
}

void TacticalGuidanceWidget::onSynced()
{
    refreshAgentList();

    importRegisteredCommandsIntoLibrary();
}

void TacticalGuidanceWidget::scheduleSaveLayout()
{
    if (layoutRestoring)
        return;
    if (layoutSaveTimer)
        layoutSaveTimer->start();
}

void TacticalGuidanceWidget::saveLayout()
{
    if (!mainHSplitter)
        return;

    QDir dir(QDir::home().filePath(".adaptix"));
    if (!dir.exists("data"))
        dir.mkpath("data");

    QJsonObject root;
    QJsonArray sizes;
    const QList<int> s = mainHSplitter->sizes();
    for (int v : s)
        sizes.append(v);
    root["splitter_sizes"] = sizes;

    if (composerTree && composerTree->header()) {
        const QByteArray st = composerTree->header()->saveState();
        root["composer_header_state"] = QString::fromLatin1(st.toBase64());
    }
    if (resultsTree && resultsTree->header()) {
        const QByteArray st = resultsTree->header()->saveState();
        root["results_header_state"] = QString::fromLatin1(st.toBase64());
    }

    QFile file(dir.absoluteFilePath("data/tactical_guidance_layout.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

bool TacticalGuidanceWidget::loadLayout()
{
    if (!mainHSplitter)
        return false;

    QDir dir(QDir::home().filePath(".adaptix"));
    const QString path = dir.absoluteFilePath("data/tactical_guidance_layout.json");
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;
    const QJsonObject root = doc.object();

    layoutRestoring = true;

    if (root.contains("splitter_sizes") && root["splitter_sizes"].isArray()) {
        const QJsonArray arr = root["splitter_sizes"].toArray();
        QList<int> sizes;
        for (const auto& v : arr)
            sizes.append(v.toInt());
        if (!sizes.isEmpty())
            mainHSplitter->setSizes(sizes);
    }

    if (composerTree && composerTree->header() && root.contains("composer_header_state")) {
        const QByteArray st = QByteArray::fromBase64(root["composer_header_state"].toString().toLatin1());
        if (!st.isEmpty())
            composerTree->header()->restoreState(st);
    }
    if (resultsTree && resultsTree->header() && root.contains("results_header_state")) {
        const QByteArray st = QByteArray::fromBase64(root["results_header_state"].toString().toLatin1());
        if (!st.isEmpty())
            resultsTree->header()->restoreState(st);
    }

    layoutRestoring = false;
    return true;
}

void TacticalGuidanceWidget::importRegisteredCommandsIntoLibrary()
{
    if (!adaptixWidget || !libraryModel)
        return;

    struct TgTaxonomyDomain {
        QString id;
        QString title;
        QString description;
        QStringList keywords;
    };

    auto taxonomyPath = []() -> QString {
        return QDir(QDir::home().filePath(".adaptix")).absoluteFilePath("data/tactical_command_taxonomy.json");
    };

    auto ensureDefaultTaxonomyFile = [&](const QString& path) {
        QFile f(path);
        if (f.exists())
            return;

        QDir dir(QDir::home().filePath(".adaptix"));
        if (!dir.exists("data"))
            dir.mkpath("data");

        QJsonObject root;
        root["schema_version"] = 1;
        root["title"] = "Tactical Command Taxonomy";
        root["description"] = "该文件用于把 Commander 中的内置命令/Extension-Kit 命令分类到 Tactical Guidance 的 Library 树中。你只需要在这里调整分类域、描述和关键词，无需改动 Client 代码。匹配逻辑：把命令路径(如 `jump psexec`) 与描述拼接成文本，然后按 domains 顺序依次用每个域的 keywords 做 contains 匹配，命中则归类到该域；否则归类到 `通用 (General)`。\n\ndomains 数组元素格式：{ id, title, description, keywords }\n- id: 稳定标识\n- title: UI 显示名称\n- description: 域说明（会显示在 UI 提示中）\n- keywords: 字符串数组（大小写不敏感，contains 匹配）\n\n注意：Client 不会在源码里内置任何关键词，domains 默认为空，你可以自行添加域定义。";

        root["domains"] = QJsonArray();

        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
        }
    };

    auto loadTaxonomy = [&](QVector<TgTaxonomyDomain>& outDomains) {
        outDomains.clear();

        const QString path = taxonomyPath();
        ensureDefaultTaxonomyFile(path);

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject())
            return;

        const QJsonObject root = doc.object();
        if (!root.contains("domains") || !root["domains"].isArray())
            return;

        const QJsonArray domains = root["domains"].toArray();
        for (const auto& dv : domains) {
            if (!dv.isObject())
                continue;
            const QJsonObject d = dv.toObject();
            TgTaxonomyDomain dom;
            dom.id = d.value("id").toString();
            dom.title = d.value("title").toString();
            dom.description = d.value("description").toString();
            if (d.contains("keywords") && d["keywords"].isArray()) {
                const QJsonArray ks = d["keywords"].toArray();
                for (const auto& kv : ks) {
                    if (kv.isString())
                        dom.keywords.append(kv.toString());
                }
            }
            if (!dom.title.isEmpty())
                outDomains.append(dom);
        }
    };

    QVector<TgTaxonomyDomain> taxonomyDomains;
    loadTaxonomy(taxonomyDomains);

    QMap<QString, QString> domainDescByTitle;
    for (const auto& d : taxonomyDomains) {
        if (!d.title.isEmpty() && !d.description.isEmpty())
            domainDescByTitle[d.title] = d.description;
    }

    auto classifyDomainTitle = [&](const QString& commandPath, const QString& description) -> QString {
        const QString hay = (commandPath + " " + description).toLower();
        for (const auto& dom : taxonomyDomains) {
            for (const auto& kw : dom.keywords) {
                const QString k = kw.toLower();
                if (k.isEmpty())
                    continue;
                if (hay.contains(k))
                    return dom.title;
            }
        }
        return QStringLiteral("通用 (General)");
    };

    auto ensureTopCategory = [&](const QString& title) -> QStandardItem* {
        QStandardItem* root = libraryModel->invisibleRootItem();
        for (int i = 0; i < root->rowCount(); ++i) {
            QStandardItem* it = root->child(i);
            if (!it)
                continue;
            if (it->data(Qt::UserRole + 1).toString() == "category" && it->text() == title)
                return it;
        }

        const QString id = QUuid::createUuid().toString();
        auto* it = new QStandardItem(title);
        it->setData(id, Qt::UserRole);
        it->setData(QStringLiteral("category"), Qt::UserRole + 1);
        it->setFont(QFont("", -1, QFont::Bold));
        it->setSelectable(false);
        it->setIcon(QIcon(":/icons/folder"));

        TacticalNodeData node;
        node.id = id;
        node.name = title;
        node.type = "category";
        node.description = "";
        node.parentId = "";
        nodeMap[id] = node;

        root->appendRow(it);
        return it;
    };

    auto clearChildren = [&](QStandardItem* parent) {
        if (!parent)
            return;
        while (parent->rowCount() > 0) {
            QStandardItem* child = parent->child(0);
            cleanupNodeData(child);
            parent->removeRow(0);
        }
    };

    auto ensureChildCategory = [&](QStandardItem* parent, const QString& title, const QString& desc = QString()) -> QStandardItem* {
        if (!parent)
            return nullptr;
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem* it = parent->child(i);
            if (!it)
                continue;
            if (it->data(Qt::UserRole + 1).toString() == "category" && it->text() == title) {
                if (!desc.isEmpty()) {
                    it->setToolTip(desc);
                    const QString id = it->data(Qt::UserRole).toString();
                    if (nodeMap.contains(id))
                        nodeMap[id].description = desc;
                }
                return it;
            }
        }

        const QString id = QUuid::createUuid().toString();
        auto* it = new QStandardItem(title);
        it->setData(id, Qt::UserRole);
        it->setData(QStringLiteral("category"), Qt::UserRole + 1);
        it->setFont(QFont("", -1, QFont::Bold));
        it->setSelectable(false);
        it->setIcon(QIcon(":/icons/folder"));
        if (!desc.isEmpty())
            it->setToolTip(desc);

        TacticalNodeData node;
        node.id = id;
        node.name = title;
        node.type = "category";
        node.description = desc;
        node.parentId = parent->data(Qt::UserRole).toString();
        nodeMap[id] = node;

        parent->appendRow(it);
        return it;
    };

    auto commandToRaw = [&](const QString& path, const Command& c) -> QString {
        if (!c.example.trimmed().isEmpty())
            return c.example;
        return path;
    };

    auto addCommandItem = [&](QStandardItem* parent, const QString& title, const QString& raw, const QString& desc, const int os) {
        if (!parent)
            return;
        const QString id = QUuid::createUuid().toString();
        auto* it = new QStandardItem(title);
        it->setData(id, Qt::UserRole);
        it->setData(QStringLiteral("command"), Qt::UserRole + 1);
        it->setData(os, ROLE_COMMAND_OS);
        if (os == OS_WINDOWS) it->setIcon(QIcon(":/icons/os_win_blue"));
        else if (os == OS_LINUX) it->setIcon(QIcon(":/icons/os_linux_blue"));
        else if (os == OS_MAC) it->setIcon(QIcon(":/icons/os_mac_blue"));
        else it->setIcon(QIcon(":/icons/code_blocks"));
        it->setToolTip(QString("%1\n\nCommand: %2").arg(desc).arg(raw));

        TacticalCommandData cmd;
        cmd.id = id;
        cmd.name = title;
        cmd.rawCommand = raw;
        cmd.description = desc;
        cmd.os = os;
        cmd.risk = 1;
        cmd.arch = 0;
        commandMap[id] = cmd;

        TacticalNodeData node;
        node.id = id;
        node.name = title;
        node.type = "command";
        node.description = desc;
        node.parentId = parent->data(Qt::UserRole).toString();
        node.command = cmd;
        nodeMap[id] = node;

        parent->appendRow(it);
    };

    QStandardItem* builtinsRoot = ensureTopCategory("内置命令库 (Built-in Commands)");
    QStandardItem* extsRoot = ensureTopCategory("扩展命令库 (Extension-Kit Commands)");
    if (!builtinsRoot || !extsRoot)
        return;

    clearChildren(builtinsRoot);
    clearChildren(extsRoot);

    QMap<int, QMap<QString, QMap<QString, QMap<QString, QPair<Command, QString>>>>> builtins;
    QMap<int, QMap<QString, QMap<QString, QMap<QString, QPair<Command, QString>>>>> exts;

    for (const auto& regAgent : adaptixWidget->RegisterAgents) {
        if (!regAgent.valid || !regAgent.commander)
            continue;
        const int os = regAgent.os;
        const QString source = regAgent.name + " / " + regAgent.listenerType;

        const CommandsGroup& regGroup = regAgent.commander->GetRegCommandsGroup();
        if (!regGroup.commands.isEmpty()) {
            const QString gname = regGroup.groupName.isEmpty() ? QStringLiteral("Core") : regGroup.groupName;
            for (const auto& cmd : regGroup.commands) {
                if (cmd.subcommands.isEmpty()) {
                    const QString path = cmd.name;
                    const QString domain = classifyDomainTitle(path, cmd.description);
                    builtins[os][domain][gname][path] = qMakePair(cmd, source);
                } else {
                    for (const auto& sc : cmd.subcommands) {
                        const QString path = cmd.name + " " + sc.name;
                        const QString domain = classifyDomainTitle(path, sc.description);
                        builtins[os][domain][gname][path] = qMakePair(sc, source);
                    }
                }
            }
        }

        const QVector<CommandsGroup>& axGroups = regAgent.commander->GetAxCommandsGroups();
        for (const auto& g : axGroups) {
            const QString gname = g.groupName.isEmpty() ? QStringLiteral("Misc") : g.groupName;
            for (const auto& cmd : g.commands) {
                if (cmd.subcommands.isEmpty()) {
                    const QString path = cmd.name;
                    const QString domain = classifyDomainTitle(path, cmd.description);
                    exts[os][domain][gname][path] = qMakePair(cmd, source);
                } else {
                    for (const auto& sc : cmd.subcommands) {
                        const QString path = cmd.name + " " + sc.name;
                        const QString domain = classifyDomainTitle(path, sc.description);
                        exts[os][domain][gname][path] = qMakePair(sc, source);
                    }
                }
            }
        }
    }

    auto osName = [](int os) -> QString {
        if (os == OS_WINDOWS) return QStringLiteral("Windows");
        if (os == OS_LINUX) return QStringLiteral("Linux");
        if (os == OS_MAC) return QStringLiteral("macOS");
        return QStringLiteral("Any");
    };

    auto buildTree = [&](QStandardItem* root, const QMap<int, QMap<QString, QMap<QString, QMap<QString, QPair<Command, QString>>>>>& data) {
        for (auto itOs = data.begin(); itOs != data.end(); ++itOs) {
            const int os = itOs.key();
            QStandardItem* osCat = ensureChildCategory(root, osName(os));
            if (!osCat)
                continue;
            for (auto itDomain = itOs.value().begin(); itDomain != itOs.value().end(); ++itDomain) {
                const QString domainTitle = itDomain.key();
                const QString domainDesc = domainDescByTitle.value(domainTitle);
                QStandardItem* domainCat = ensureChildCategory(osCat, domainTitle, domainDesc);
                if (!domainCat)
                    continue;
                for (auto itGroup = itDomain.value().begin(); itGroup != itDomain.value().end(); ++itGroup) {
                    QStandardItem* grpCat = ensureChildCategory(domainCat, itGroup.key());
                    if (!grpCat)
                        continue;
                    for (auto itCmd = itGroup.value().begin(); itCmd != itGroup.value().end(); ++itCmd) {
                        const QString path = itCmd.key();
                        const Command& c = itCmd.value().first;
                        const QString source = itCmd.value().second;
                        const QString raw = commandToRaw(path, c);
                        const QString desc = (c.description.isEmpty() ? QString() : c.description) + (source.isEmpty() ? QString() : (QStringLiteral(" | Source: ") + source));
                        addCommandItem(grpCat, path, raw, desc, os);
                    }
                }
            }
        }
    };

    buildTree(builtinsRoot, builtins);
    buildTree(extsRoot, exts);

    libraryView->expandAll();
    saveLibrary();
}

void TacticalGuidanceWidget::createLibraryUI()
{
    libraryPanel = new QWidget(this);
    libraryLayout = new QVBoxLayout(libraryPanel);
    libraryLayout->setContentsMargins(4, 4, 4, 4);
    libraryLayout->setSpacing(4);

    libraryPanel->setMinimumWidth(300);

    // --- Agent Selection Section ---
    libraryLayout->addWidget(new QLabel("<b>Target Agents</b>"));
    
    agentSelectBtn = new QPushButton("Select Agents (0)", libraryPanel);
    agentSelectMenu = new StickyCheckMenu(this);
    agentSelectBtn->setMenu(agentSelectMenu);
    libraryLayout->addWidget(agentSelectBtn);

    connect(agentSelectMenu, &QMenu::triggered, this, &TacticalGuidanceWidget::onAgentMenuTriggered);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("OS Filter:"));
    agentOsFilter = new QComboBox(libraryPanel);
    agentOsFilter->addItems({"Auto (Based on Selection)", "Windows", "Linux", "macOS", "All"});
    filterLayout->addWidget(agentOsFilter);
    libraryLayout->addLayout(filterLayout);

    connect(agentOsFilter, &QComboBox::currentTextChanged, this, &TacticalGuidanceWidget::onAgentSelectionChanged); // Re-filter on change

    // --- Library Section ---
    libraryLayout->addWidget(new QLabel("<b>Tactical Library</b>"));
    
    librarySearch = new QLineEdit(libraryPanel);
    librarySearch->setPlaceholderText("Search Command...");
    libraryLayout->addWidget(librarySearch);

    libraryView = new QTreeView(libraryPanel);
    libraryView->setHeaderHidden(true);
    libraryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    libraryView->setDragEnabled(true);
    libraryView->setDragDropMode(QAbstractItemView::DragOnly);
    libraryView->viewport()->installEventFilter(this);
    libraryView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(libraryView, &QTreeView::customContextMenuRequested, this, &TacticalGuidanceWidget::onLibraryContextMenu);

    libraryModel = new QStandardItemModel(this);
    libraryProxyModel = new TacticalLibraryProxyModel(this);
    libraryProxyModel->setSourceModel(libraryModel);
    libraryProxyModel->setRecursiveFilteringEnabled(true);
    libraryProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    
    libraryView->setModel(libraryProxyModel);

    libraryLayout->addWidget(libraryView, 1);

    mainHSplitter->addWidget(libraryPanel);

    connect(librarySearch, &QLineEdit::textChanged, this, &TacticalGuidanceWidget::onLibrarySearchChanged);
    connect(libraryView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TacticalGuidanceWidget::onLibraryBlockSelected);
    
    // Initial population of agents (if any)
    refreshAgentList();
}

void TacticalGuidanceWidget::createComposerUI()
{
    composerPanel = new QWidget(this);
    composerLayout = new QVBoxLayout(composerPanel);
    composerLayout->setContentsMargins(4, 4, 4, 4);
    composerLayout->setSpacing(4);

    composerPanel->setMinimumWidth(420);

    composerLayout->addWidget(new QLabel("<b>Playbook Composer</b>"));

    // --- Playbook Selection Row ---
    QHBoxLayout* pbLayout = new QHBoxLayout();
    pbLayout->addWidget(new QLabel("Playbook:"));
    
    playbookSelector = new QComboBox(composerPanel);
    playbookSelector->setEditable(false);
    connect(playbookSelector, &QComboBox::currentTextChanged, this, &TacticalGuidanceWidget::onWorkflowSelected);
    pbLayout->addWidget(playbookSelector, 1);

    QPushButton* btnNewPb = new QPushButton(composerPanel);
    btnNewPb->setIcon(QIcon(":/icons/file_open"));
    btnNewPb->setToolTip("New Playbook");
    btnNewPb->setMaximumWidth(30);
    connect(btnNewPb, &QPushButton::clicked, this, &TacticalGuidanceWidget::onAddPlaybookClicked);
    pbLayout->addWidget(btnNewPb);

    QPushButton* btnDelPb = new QPushButton(composerPanel);
    btnDelPb->setIcon(QIcon(":/icons/close"));
    btnDelPb->setToolTip("Delete Playbook");
    btnDelPb->setMaximumWidth(30);
    connect(btnDelPb, &QPushButton::clicked, this, [this](){
        if (playbooks.isEmpty())
            return;
        if (QMessageBox::question(this, "Delete", "Delete current playbook?") != QMessageBox::Yes)
            return;
        storeCurrentPlaybook();

        QJsonArray newArr;
        for (const auto& v : playbooks) {
            QJsonObject pb = v.toObject();
            if (pb["id"].toString() == currentPlaybookId)
                continue;
            newArr.append(pb);
        }
        playbooks = newArr;
        ensureDefaultPlaybook();
        rebuildPlaybookList();
        switchPlaybook(currentPlaybookId);
        saveComposer();
    });
    pbLayout->addWidget(btnDelPb);

    QPushButton* btnOptsPb = new QPushButton(composerPanel);
    btnOptsPb->setIcon(QIcon(":/icons/arrow_drop_down"));
    btnOptsPb->setToolTip("Playbook Options");
    btnOptsPb->setMaximumWidth(30);
    QMenu* pbMenu = new QMenu(btnOptsPb);
    
    pbMenu->addAction("Rename Playbook", this, [this](){
        if (currentPlaybookId.isEmpty())
            return;

        QString currentName;
        for (const auto& v : playbooks) {
            QJsonObject pb = v.toObject();
            if (pb["id"].toString() == currentPlaybookId) {
                currentName = pb["name"].toString();
                break;
            }
        }

        bool ok;
        QString text = QInputDialog::getText(this, "Rename Playbook", "Name:", QLineEdit::Normal, currentName, &ok);
        if (!ok || text.isEmpty())
            return;

        storeCurrentPlaybook();
        QJsonArray newArr;
        for (const auto& v : playbooks) {
            QJsonObject pb = v.toObject();
            if (pb["id"].toString() == currentPlaybookId)
                pb["name"] = text;
            newArr.append(pb);
        }
        playbooks = newArr;
        rebuildPlaybookList();
        saveComposer();
    });
    
    pbMenu->addAction("Duplicate Playbook", this, [this](){
        if (currentPlaybookId.isEmpty())
            return;
        storeCurrentPlaybook();

        QJsonObject cur;
        for (const auto& v : playbooks) {
            QJsonObject pb = v.toObject();
            if (pb["id"].toString() == currentPlaybookId) {
                cur = pb;
                break;
            }
        }
        if (cur.isEmpty())
            return;

        QJsonObject dup = cur;
        dup["id"] = QUuid::createUuid().toString();
        dup["name"] = cur["name"].toString() + " Copy";
        playbooks.append(dup);
        currentPlaybookId = dup["id"].toString();
        rebuildPlaybookList();
        switchPlaybook(currentPlaybookId);
        saveComposer();
    });
    
    btnOptsPb->setMenu(pbMenu);
    pbLayout->addWidget(btnOptsPb);

    composerLayout->addLayout(pbLayout);

    rebuildPlaybookList();

    // Playbook Settings
    QHBoxLayout* settingsLayout = new QHBoxLayout();
    settingsLayout->addWidget(new QLabel("Interval (s):"));
    workflowInterval = new QSpinBox(composerPanel);
    workflowInterval->setRange(0, 3600);
    workflowInterval->setValue(1);
    settingsLayout->addWidget(workflowInterval);
    
    // Stop on Error Checkbox
    chkStopOnError = new QCheckBox("Stop on Error", composerPanel);
    chkStopOnError->setChecked(true);
    settingsLayout->addWidget(chkStopOnError);
    
    settingsLayout->addStretch();
    composerLayout->addLayout(settingsLayout);

    composerTree = new QTreeWidget(composerPanel);
    composerTree->setHeaderLabels({"Step Name", "Details", "Raw Command"});
    composerTree->setDragEnabled(true);
    composerTree->setAcceptDrops(true);
    composerTree->setDropIndicatorShown(true);
    composerTree->setDragDropMode(QAbstractItemView::InternalMove);
    composerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    composerTree->setDefaultDropAction(Qt::MoveAction);
    composerTree->viewport()->installEventFilter(this);
    composerTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(composerTree, &QTreeWidget::customContextMenuRequested, this, &TacticalGuidanceWidget::onComposerContextMenu);

    composerTree->setItemDelegateForColumn(2, new RawCommandEditorDelegate(composerTree));

    composerTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(composerTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int column) {
        if (!item || !composerTree)
            return;
        if (column != 2)
            return;
        const QString type = item->data(4, Qt::UserRole).toString();
        if (type != "command")
            return;
        // Reduce chance of visual artifacts by forcing a repaint before editor creation
        composerTree->viewport()->update(composerTree->visualItemRect(item));
        composerTree->editItem(item, 2);
    });

    composerTree->setColumnWidth(0, 240);
    composerTree->setColumnWidth(1, 300);
    composerTree->setColumnWidth(2, 480);
    composerTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    composerTree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    composerTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    composerTree->header()->setStretchLastSection(true);

    connect(composerTree->model(), &QAbstractItemModel::rowsInserted, this, &TacticalGuidanceWidget::onComposerChanged);
    connect(composerTree->model(), &QAbstractItemModel::rowsRemoved, this, &TacticalGuidanceWidget::onComposerChanged);
    connect(composerTree->model(), &QAbstractItemModel::rowsMoved, this, &TacticalGuidanceWidget::onComposerChanged);
    connect(composerTree->model(), &QAbstractItemModel::modelReset, this, &TacticalGuidanceWidget::onComposerChanged);
    // Also track data changes (e.g. edits)
    connect(composerTree->model(), &QAbstractItemModel::dataChanged, this, &TacticalGuidanceWidget::onComposerChanged);

    composerLayout->addWidget(composerTree, 1);

    // Actions
    composerActionsRow = new QWidget(composerPanel);
    auto* actionsLayout = new QHBoxLayout(composerActionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);

    auto* btnRun = new QPushButton("Run Playbook", composerActionsRow);
    connect(btnRun, &QPushButton::clicked, this, &TacticalGuidanceWidget::runActivePlaybook);

    auto* btnClear = new QPushButton("Clear Playbook", composerActionsRow);
    connect(btnClear, &QPushButton::clicked, this, &TacticalGuidanceWidget::clearWorkflow);
    
    btnSaveLocal = new QPushButton("Save Local", composerActionsRow);
    connect(btnSaveLocal, &QPushButton::clicked, this, &TacticalGuidanceWidget::onSaveLocalClicked);

    btnPushServer = new QPushButton("Sync to Server", composerActionsRow);
    connect(btnPushServer, &QPushButton::clicked, this, &TacticalGuidanceWidget::onPushWorkflowClicked);

    actionsLayout->addWidget(btnRun);
    actionsLayout->addWidget(btnClear);
    actionsLayout->addWidget(btnSaveLocal);
    actionsLayout->addWidget(btnPushServer);

    auto* btnUp = new QPushButton(composerActionsRow);
    btnUp->setIcon(QIcon(":/icons/arrow_drop_up"));
    btnUp->setToolTip("Move Up");
    btnUp->setMaximumWidth(30);
    connect(btnUp, &QPushButton::clicked, this, [this](){ moveSelectedComposerItem(-1); });
    actionsLayout->addWidget(btnUp);

    auto* btnDown = new QPushButton(composerActionsRow);
    btnDown->setIcon(QIcon(":/icons/arrow_drop_down"));
    btnDown->setToolTip("Move Down");
    btnDown->setMaximumWidth(30);
    connect(btnDown, &QPushButton::clicked, this, [this](){ moveSelectedComposerItem(1); });
    actionsLayout->addWidget(btnDown);

    actionsLayout->addStretch();

    composerLayout->addWidget(composerActionsRow);

    mainHSplitter->addWidget(composerPanel);

    if (btnPushServer) btnPushServer->setEnabled(false);
}

void TacticalGuidanceWidget::refreshPlaybookList()
{
    rebuildPlaybookList();
}

void TacticalGuidanceWidget::onAddPlaybookClicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, "New Playbook", "Name:", QLineEdit::Normal, "New Playbook", &ok);
    if (!ok || text.isEmpty())
        return;

    storeCurrentPlaybook();

    QJsonObject pb;
    pb["id"] = QUuid::createUuid().toString();
    pb["name"] = text;
    pb["nodes"] = QJsonArray();
    playbooks.append(pb);
    currentPlaybookId = pb["id"].toString();

    rebuildPlaybookList();
    switchPlaybook(currentPlaybookId);
    saveComposer();
}

void TacticalGuidanceWidget::onWorkflowSelected()
{
    if (!playbookSelector)
        return;
    const QString pbId = playbookSelector->currentData().toString();
    if (pbId.isEmpty())
        return;
    if (pbId == currentPlaybookId)
        return;
    switchPlaybook(pbId);
}

void TacticalGuidanceWidget::createResultsUI()
{
    resultsPanel = new QWidget(this);
    resultsLayout = new QVBoxLayout(resultsPanel);
    resultsLayout->setContentsMargins(4, 4, 4, 4);
    resultsLayout->setSpacing(4);

    resultsPanel->setMinimumWidth(320);

    resultsTree = new QTreeWidget(resultsPanel);
    resultsTree->setHeaderLabels({"Agent", "Step", "TaskId", "Status"});
    resultsTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTree->setAlternatingRowColors(true);
    resultsTree->setColumnWidth(0, 90);   // Agent column
    resultsTree->setColumnWidth(1, 220);  // Step column
    resultsTree->setColumnWidth(2, 140);  // TaskId column
    resultsTree->setColumnWidth(3, 70);   // Status column
    resultsTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    resultsTree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    resultsTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    resultsTree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    resultsTree->header()->setStretchLastSection(false);

    resultsLayout->addWidget(new QLabel("<b>Results</b>"));
    
    // Actions
    QHBoxLayout* resActions = new QHBoxLayout();
    resActions->addStretch();
    
    btnClearResults = new QPushButton("Clear Results", resultsPanel);
    connect(btnClearResults, &QPushButton::clicked, this, &TacticalGuidanceWidget::onClearResultsClicked);
    resActions->addWidget(btnClearResults);
    
    resultsLayout->addLayout(resActions);
    resultsLayout->addWidget(resultsTree, 1);

    mainHSplitter->addWidget(resultsPanel);

    connect(resultsTree, &QTreeWidget::itemClicked, this, &TacticalGuidanceWidget::onResultsItemClicked);
}

void TacticalGuidanceWidget::onClearResultsClicked()
{
    resultsTree->clear();
    taskIdToResultsItem.clear();
    resultsAgentItems.clear();
}

void TacticalGuidanceWidget::buildLibraryTree(QStandardItem* parentItem, const QVector<TacticalNodeData>& nodes)
{
    QString pId = "";
    if (parentItem) pId = parentItem->data(Qt::UserRole).toString();

    for (const auto& node : nodes) {
        TacticalNodeData mutableNode = node;
        // Ensure parentId is consistent
        if (mutableNode.parentId.isEmpty()) mutableNode.parentId = pId;
        
        auto* item = new QStandardItem(mutableNode.name);
        item->setData(mutableNode.id, Qt::UserRole);
        item->setData(mutableNode.type, Qt::UserRole + 1); // Store type
        
        nodeMap[mutableNode.id] = mutableNode; // Store in map for lookup

        if (mutableNode.type == "category") {
            item->setFont(QFont("", -1, QFont::Bold));
            item->setSelectable(false);
            item->setIcon(QIcon(":/icons/folder")); 
            
            // Recursively build children
            buildLibraryTree(item, mutableNode.children);
        } 
        else if (mutableNode.type == "command") {
            commandMap[mutableNode.id] = mutableNode.command; // Store command data
            item->setToolTip(QString("%1\n\nCommand: %2").arg(mutableNode.description).arg(mutableNode.command.rawCommand));
            
            int os = mutableNode.command.os;
            item->setData(os, ROLE_COMMAND_OS);
            if (os == 1) item->setIcon(QIcon(":/icons/os_win_blue"));
            else if (os == 2) item->setIcon(QIcon(":/icons/os_linux_blue"));
            else if (os == 3) item->setIcon(QIcon(":/icons/os_mac_blue"));
        }

        if (parentItem) parentItem->appendRow(item);
        else libraryModel->appendRow(item);
    }
}

void TacticalGuidanceWidget::initDefaultLibrary()
{
    libraryModel->clear();
    nodeMap.clear();
    commandMap.clear();

    QVector<TacticalNodeData> rootNodes;

    // Helper to create a node
    auto createNode = [](const QString& name, const QString& type, const QString& desc) -> TacticalNodeData {
        TacticalNodeData node;
        node.id = QUuid::createUuid().toString();
        node.name = name;
        node.type = type;
        node.description = desc;
        return node;
    };

    auto createCommand = [&](const QString& name, const QString& cmdStr, const QString& desc, int os) -> TacticalNodeData {
        TacticalNodeData node = createNode(name, "command", desc);
        node.command.id = node.id;
        node.command.name = name;
        node.command.rawCommand = cmdStr;
        node.command.description = desc;
        node.command.os = os;
        node.command.risk = 1;
        return node;
    };

    // --- 1. Recon ---
    auto catRecon = createNode("信息收集 (Recon)", "category", "");
    
    // Level 2: System
    auto catSystem = createNode("系统信息 (System)", "category", "");
    catSystem.children.append(createCommand("基本信息", "whoami /all && ipconfig /all && systeminfo", "Basic Info", 1));
    catSystem.children.append(createCommand("基本信息", "id && ifconfig && uname -a", "Basic Info", 2));
    catRecon.children.append(catSystem);

    // Level 2: Process
    auto catProcess = createNode("进程发现 (Process)", "category", "");
    catProcess.children.append(createCommand("详细进程列表", "tasklist /v", "Process List", 1));
    catProcess.children.append(createCommand("进程树", "ps auxf", "Process Tree", 2));
    catRecon.children.append(catProcess);

    // Level 2: Network (Mixed content example)
    auto catNet = createNode("网络发现 (Network)", "category", "");
    catNet.children.append(createCommand("ARP缓存", "arp -a", "ARP Table", 1));
    
    // Level 3: Deep Scan
    auto catDeep = createNode("深度扫描 (Deep)", "category", "");
    catDeep.children.append(createCommand("全端口扫描", "nmap -p- 127.0.0.1", "Full Scan", 2));
    catNet.children.append(catDeep);
    
    catRecon.children.append(catNet);

    rootNodes.append(catRecon);

    // --- 2. Persistence ---
    auto catPersist = createNode("权限维持 (Persistence)", "category", "");
    catPersist.children.append(createCommand("HKCU Run键", "reg add HKCU\\... /f", "Registry Run", 1));
    catPersist.children.append(createCommand("User Crontab", "(crontab -l; ...)|crontab -", "Cron Job", 2));
    rootNodes.append(catPersist);

    // --- 3. Creds ---
    auto catCreds = createNode("凭据获取 (Credential Access)", "category", "");
    catCreds.children.append(createCommand("Procdump", "procdump.exe -ma lsass.exe", "Dump LSASS", 1));
    rootNodes.append(catCreds);

    buildLibraryTree(nullptr, rootNodes);
    libraryView->expandAll();
}

void TacticalGuidanceWidget::refreshAgentList()
{
    // Save current selection
    QSet<QString> selectedIds;
    for (auto* action : agentSelectMenu->actions()) {
        if (action->isChecked()) {
            selectedIds.insert(action->data().toString());
        }
    }

    agentSelectMenu->clear();
    
    if (!adaptixWidget) return;

    for (auto it = adaptixWidget->AgentsMap.begin(); it != adaptixWidget->AgentsMap.end(); ++it) {
        Agent* agent = it.value();
        // Format: ID - IP - Username
        QString label = QString("%1 - %2 - %3").arg(agent->data.Id).arg(agent->data.InternalIP).arg(agent->data.Username);
        
        QAction* action = agentSelectMenu->addAction(label);
        action->setCheckable(true);
        action->setData(agent->data.Id);
        
        if (selectedIds.contains(agent->data.Id)) {
            action->setChecked(true);
        }

        // Icon based on OS
        if (agent->data.OsDesc.contains("Windows", Qt::CaseInsensitive)) action->setIcon(QIcon(":/icons/os_win"));
        else if (agent->data.OsDesc.contains("Linux", Qt::CaseInsensitive)) action->setIcon(QIcon(":/icons/os_linux"));
        else action->setIcon(QIcon(":/icons/os_mac"));
    }
    
    // Update button text
    int count = 0;
    for(auto* action : agentSelectMenu->actions()) if(action->isChecked()) count++;
    agentSelectBtn->setText(QString("Select Agents (%1)").arg(count));
    
    onAgentSelectionChanged();
}

void TacticalGuidanceWidget::onAgentMenuTriggered(QAction* action)
{
    Q_UNUSED(action);
    int count = 0;
    for(auto* a : agentSelectMenu->actions()) if(a->isChecked()) count++;
    agentSelectBtn->setText(QString("Select Agents (%1)").arg(count));
    
    onAgentSelectionChanged();
}

void TacticalGuidanceWidget::onAgentSelectionChanged()
{
    QString filterMode = agentOsFilter->currentText();
    int targetOs = 0; // 0=All
    
    if (filterMode == "Auto (Based on Selection)") {
        // Check selected agents
        int win = 0, linux = 0, mac = 0;
        
        for (auto* action : agentSelectMenu->actions()) {
            if (!action->isChecked()) continue;
            
            // We need to look up agent data really, but using label text heuristic for now as we did before
            // Or better, look up in AdaptixWidget
            QString id = action->data().toString();
            if(adaptixWidget->AgentsMap.contains(id)) {
                Agent* agent = adaptixWidget->AgentsMap[id];
                if(agent->data.OsDesc.contains("Windows", Qt::CaseInsensitive)) win++;
                else if(agent->data.OsDesc.contains("Linux", Qt::CaseInsensitive)) linux++;
                else if(agent->data.OsDesc.contains("Darwin", Qt::CaseInsensitive) || agent->data.OsDesc.contains("Mac", Qt::CaseInsensitive)) mac++;
            }
        }

        if (win > 0 && linux == 0 && mac == 0) targetOs = 1;
        else if (win == 0 && linux > 0 && mac == 0) targetOs = 2;
        else if (win == 0 && linux == 0 && mac > 0) targetOs = 3;
        else targetOs = 0; // Mixed or none
        
    } else if (filterMode == "Windows") targetOs = 1;
    else if (filterMode == "Linux") targetOs = 2;
    else if (filterMode == "macOS") targetOs = 3;
    
    // Filter Library View
    if (libraryProxyModel) {
        auto* proxy = static_cast<TacticalLibraryProxyModel*>(libraryProxyModel);
        proxy->setOsFilter(targetOs);
    }
}

void TacticalGuidanceWidget::onPushWorkflowClicked()
{
    syncWorkflowToServer();
}

void TacticalGuidanceWidget::onSaveLocalClicked()
{
    saveComposer();
}

void TacticalGuidanceWidget::syncWorkflowToServer()
{
    // L4 + A mode: server workflow sync is disabled.
}

void TacticalGuidanceWidget::handleWorkflowSync(const QJsonObject& json)
{
    Q_UNUSED(json);
    // L4 + A mode: server workflow sync is disabled.
}

QString TacticalGuidanceWidget::riskLabel(const int risk) const
{
    if (risk == 1)
        return "Low";
    if (risk == 2)
        return "Medium";
    if (risk == 3)
        return "High";
    return "Unknown";
}

QStringList TacticalGuidanceWidget::collectSelectedAgentIds() const
{
    QStringList ids;
    if (!agentSelectMenu)
        return ids;

    for (auto* action : agentSelectMenu->actions()) {
        if (action && action->isChecked())
            ids.push_back(action->data().toString());
    }
    return ids;
}

QList<QTreeWidgetItem*> TacticalGuidanceWidget::collectCommandSteps() const
{
    QList<QTreeWidgetItem*> steps;
    if (!composerTree)
        return steps;

    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) {
        if (!item)
            return;
        const QString type = item->data(4, Qt::UserRole).toString();
        if (type == "command")
            steps.push_back(item);
        for (int i = 0; i < item->childCount(); ++i)
            walk(item->child(i));
    };

    for (int i = 0; i < composerTree->topLevelItemCount(); ++i)
        walk(composerTree->topLevelItem(i));

    return steps;
}

void TacticalGuidanceWidget::stopExecution()
{
    executionRunning = false;
    executionTargetAgents.clear();
    taskIdToComposerItem.clear();
    taskIdToResultsItem.clear();
    resultsAgentItems.clear();
    agentQueues.clear();
}

void TacticalGuidanceWidget::finalizeExecutionIfDone()
{
    if (!executionRunning)
        return;

    for (auto it = agentQueues.begin(); it != agentQueues.end(); ++it) {
        const AgentExecutionQueue& q = it.value();
        if (q.isWaitingForTask)
            return;
        if (q.currentCommand != nullptr)
            return;
        if (!q.currentTaskId.isEmpty())
            return;
        if (!q.commandQueue.isEmpty())
            return;
    }

    TG_DEBUG << "[TG] All agent queues finished. Stopping execution.";
    stopExecution();
}

void TacticalGuidanceWidget::startExecutionWithSteps(const QList<QTreeWidgetItem*>& steps)
{
    if (executionRunning) {
        TG_DEBUG << "[TG] Execution already running, ignoring request";
        return;
    }

    executionTargetAgents = collectSelectedAgentIds();
    if (executionTargetAgents.isEmpty()) {
        QMessageBox::information(this, "Tactical", "Please select at least one agent.");
        return;
    }

    if (steps.isEmpty()) {
        QMessageBox::information(this, "Tactical", "No command steps in playbook.");
        return;
    }

    TG_DEBUG << "[TG] === Starting Tactical Guidance Execution ===";
    TG_DEBUG << "[TG] Selected agents:" << executionTargetAgents;
    TG_DEBUG << "[TG] Found" << steps.size() << "command steps to execute";

    // Initialize per-agent queues
    initializeAgentQueues();

    // Clear old mappings
    taskIdToComposerItem.clear();
    taskIdToResultsItem.clear();
    resultsAgentItems.clear();

    // Reuse existing agent groups in results tree so runs accumulate.
    // Build a fresh mapping for current run based on existing top-level items.
    if (resultsTree) {
        for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* top = resultsTree->topLevelItem(i);
            if (!top)
                continue;
            const QString id = top->text(0);
            if (!id.isEmpty())
                resultsAgentItems[id] = top;
        }
    }

    // Ensure agent groups exist
    for (const QString& agentId : executionTargetAgents) {
        QTreeWidgetItem* agentItem = resultsAgentItems.value(agentId, nullptr);
        if (!agentItem && resultsTree) {
            agentItem = new QTreeWidgetItem(resultsTree);
            agentItem->setText(0, agentId);
            agentItem->setText(1, "");
            agentItem->setText(2, "");
            agentItem->setText(3, "");
            resultsAgentItems[agentId] = agentItem;
        }
        if (agentItem)
            agentItem->setExpanded(true);
    }

    // Build command queues for each agent (results rows are created at execution time)
    for (QTreeWidgetItem* stepItem : steps) {
        if (!stepItem)
            continue;
        for (const QString& agentId : executionTargetAgents) {
            AgentExecutionQueue& queue = agentQueues[agentId];
            queue.commandQueue.append(stepItem);
        }
    }

    executionRunning = true;

    // Start execution for all agents
    TG_DEBUG << "[TG] Starting execution for" << executionTargetAgents.size() << "agents";
    for (const QString& agentId : executionTargetAgents) {
        TG_DEBUG << "[TG] Starting execution for agent:" << agentId;
        executeNextCommandForAgent(agentId);
    }
}

void TacticalGuidanceWidget::initializeAgentQueues()
{
    agentQueues.clear();
    for (const QString& agentId : executionTargetAgents) {
        AgentExecutionQueue queue;
        queue.agentId = agentId;
        queue.isWaitingForTask = false;
        queue.currentStepIndex = 0;
        agentQueues[agentId] = queue;
        TG_DEBUG << "[TG] Initialized queue for agent:" << agentId;
    }
}

void TacticalGuidanceWidget::executeNextCommandForAgent(const QString& agentId)
{
    if (!agentQueues.contains(agentId)) {
        TG_DEBUG << "[TG] No queue found for agent:" << agentId;
        return;
    }

    AgentExecutionQueue& queue = agentQueues[agentId];
    
    if (queue.isWaitingForTask) {
        TG_DEBUG << "[TG] Agent" << agentId << "is already waiting for task completion";
        return;
    }

    if (queue.commandQueue.isEmpty()) {
        TG_DEBUG << "[TG] No more commands for agent:" << agentId;
        queue.currentCommand = nullptr;
        queue.currentResultsItem = nullptr;
        queue.currentTaskId.clear();
        queue.isWaitingForTask = false;
        finalizeExecutionIfDone();
        return;
    }

    QTreeWidgetItem* nextCommand = queue.commandQueue.takeFirst();
    if (!nextCommand) {
        TG_DEBUG << "[TG] Invalid command item for agent:" << agentId;
        return;
    }

    queue.currentCommand = nextCommand;
    queue.currentResultsItem = nullptr;
    queue.isWaitingForTask = true;
    queue.currentStepIndex++;

    // Create a new Results row for this command so Results accumulate across runs.
    QTreeWidgetItem* agentItem = resultsAgentItems.value(agentId, nullptr);
    if (!agentItem && resultsTree) {
        // Fallback: rebuild mapping from existing top-level items
        for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* top = resultsTree->topLevelItem(i);
            if (top && top->text(0) == agentId) {
                agentItem = top;
                resultsAgentItems[agentId] = agentItem;
                break;
            }
        }
    }
    if (!agentItem && resultsTree) {
        agentItem = new QTreeWidgetItem(resultsTree);
        agentItem->setText(0, agentId);
        agentItem->setText(1, "");
        agentItem->setText(2, "");
        agentItem->setText(3, "");
        resultsAgentItems[agentId] = agentItem;
    }
    if (agentItem) {
        QTreeWidgetItem* taskItem = new QTreeWidgetItem(agentItem);
        taskItem->setText(0, "");
        taskItem->setText(1, nextCommand->text(0));
        taskItem->setText(2, "");
        taskItem->setText(3, "Pending");
        agentItem->setExpanded(true);
        queue.currentResultsItem = taskItem;
    }

    TG_DEBUG << "[TG] === KEY DEBUG ===";
    TG_DEBUG << "[TG] Executing command" << queue.currentStepIndex << "for agent:" << agentId;
    TG_DEBUG << "[TG] Command ID:" << nextCommand->data(2, Qt::UserRole).toString();
    TG_DEBUG << "[TG] Queue size after taking command:" << queue.commandQueue.size() << "for agent:" << agentId;
    TG_DEBUG << "[TG] =================";

    const int intervalSec = workflowInterval ? workflowInterval->value() : 0;
    const int delayMs = intervalSec * 1000;
    const bool applyDelay = (delayMs > 0 && queue.currentStepIndex > 1);
    if (applyDelay) {
        TG_DEBUG << "[TG] Applying interval delay (ms):" << delayMs << "agent:" << agentId;
        QTimer::singleShot(delayMs, this, [this, agentId, nextCommand]() {
            if (!executionRunning)
                return;
            if (!agentQueues.contains(agentId))
                return;
            AgentExecutionQueue& q = agentQueues[agentId];
            if (q.currentCommand != nextCommand)
                return;
            submitCommandForAgent(agentId, nextCommand);
        });
        return;
    }

    submitCommandForAgent(agentId, nextCommand);
}

void TacticalGuidanceWidget::submitCommandForAgent(const QString& agentId, QTreeWidgetItem* commandItem)
{
    if (!adaptixWidget || !adaptixWidget->AgentsMap.contains(agentId)) {
        TG_DEBUG << "[TG] Agent not found:" << agentId;
        if (agentQueues.contains(agentId)) {
            AgentExecutionQueue& queue = agentQueues[agentId];
            queue.isWaitingForTask = false;
            queue.currentTaskId.clear();
            queue.currentCommand = nullptr;
            finalizeExecutionIfDone();
        }
        return;
    }

    Agent* agent = adaptixWidget->AgentsMap[agentId];
    if (!agent || !agent->commander) {
        TG_DEBUG << "[TG] Agent or commander not initialized:" << agentId;
        AgentExecutionQueue& queue = agentQueues[agentId];
        queue.isWaitingForTask = false;
        queue.currentResultsItem = nullptr;
        queue.currentTaskId.clear();
        queue.currentCommand = nullptr;
        finalizeExecutionIfDone();
        return;
    }

    const QString commandId = commandItem->data(2, Qt::UserRole).toString();
    if (!commandMap.contains(commandId)) {
        TG_DEBUG << "[TG] Command not found:" << commandId;
        AgentExecutionQueue& queue = agentQueues[agentId];
        queue.isWaitingForTask = false;
        queue.currentTaskId.clear();
        queue.currentCommand = nullptr;
        finalizeExecutionIfDone();
        return;
    }

    const auto& cmd = commandMap[commandId];
    QString commandLine = commandItem ? commandItem->text(2) : QString();
    if (commandLine.trimmed().isEmpty())
        commandLine = cmd.rawCommand;

    QTreeWidgetItem* agentResItem = nullptr;
    if (agentQueues.contains(agentId))
        agentResItem = agentQueues[agentId].currentResultsItem;
    if (!agentResItem) {
        QTreeWidgetItem* agentItem = resultsAgentItems.value(agentId, nullptr);
        if (agentItem) {
            agentResItem = new QTreeWidgetItem(agentItem);
            agentResItem->setText(0, "");
            agentResItem->setText(1, commandItem->text(0));
            agentResItem->setText(2, "");
            agentResItem->setText(3, "Pending");
            agentItem->setExpanded(true);
            if (agentQueues.contains(agentId))
                agentQueues[agentId].currentResultsItem = agentResItem;
        }
    }

    QTreeWidgetItem* commandItemForCallback = commandItem;

    if (!agent->Console) {
        TG_DEBUG << "[TG] Console is not initialized for agent:" << agentId;
        AgentExecutionQueue& queue = agentQueues[agentId];
        queue.isWaitingForTask = false;
        queue.currentTaskId.clear();
        queue.currentCommand = nullptr;
        if (agentResItem) {
            agentResItem->setText(3, "Submit Error");
        }
        if (chkStopOnError && chkStopOnError->isChecked()) {
            stopExecution();
        } else {
            QTimer::singleShot(0, this, [this, agentId]() {
                if (executionRunning)
                    executeNextCommandForAgent(agentId);
            });
            finalizeExecutionIfDone();
        }
        return;
    }

    CommandSubmitCallback submitCb = [this, agentId, commandItemForCallback](const CommandSubmitInfo& info) {
        TG_DEBUG << "[TG] Submit callback for agent" << agentId
                 << "ok:" << info.ok << "handlerId:" << info.handlerId << "taskId:" << info.taskId;

        if (!agentQueues.contains(agentId))
            return;

        AgentExecutionQueue& queue = agentQueues[agentId];
        if (queue.currentCommand != commandItemForCallback)
            return;

        if (!info.ok) {
            QTreeWidgetItem* resItem = queue.currentResultsItem;
            queue.isWaitingForTask = false;
            queue.currentResultsItem = nullptr;
            queue.currentTaskId.clear();
            queue.currentCommand = nullptr;

            if (resItem)
                resItem->setText(3, "Submit Error");

            QTimer::singleShot(0, this, [this, agentId]() {
                if (!executionRunning)
                    return;
                if (chkStopOnError && chkStopOnError->isChecked()) {
                    stopExecution();
                    return;
                }
                executeNextCommandForAgent(agentId);
            });
            finalizeExecutionIfDone();
            return;
        }

        if (!info.taskId.isEmpty()) {
            if (queue.currentTaskId.isEmpty())
                queue.currentTaskId = info.taskId;

            if (commandItemForCallback)
                taskIdToComposerItem[info.taskId] = commandItemForCallback;

            if (queue.currentResultsItem) {
                queue.currentResultsItem->setText(2, info.taskId);
                queue.currentResultsItem->setText(3, "Running");
                queue.currentResultsItem->setData(0, Qt::UserRole, info.taskId);
                taskIdToResultsItem[info.taskId] = queue.currentResultsItem;
            }
        }
    };

    TaskIdCallback taskIdCb = [this, agentId, commandItemForCallback](const QString& handlerId, const QString& taskId) {
        TG_DEBUG << "[TG] TaskIdCallback received for agent" << agentId
                 << "handlerId:" << handlerId << "taskId:" << taskId;

        if (taskId.isEmpty())
            return;

        if (!agentQueues.contains(agentId))
            return;

        AgentExecutionQueue& queue = agentQueues[agentId];
        if (queue.currentCommand != commandItemForCallback)
            return;

        if (queue.currentTaskId.isEmpty())
            queue.currentTaskId = taskId;

        if (commandItemForCallback)
            taskIdToComposerItem[taskId] = commandItemForCallback;

        if (queue.currentResultsItem) {
            queue.currentResultsItem->setText(2, taskId);
            queue.currentResultsItem->setText(3, "Running");
            queue.currentResultsItem->setData(0, Qt::UserRole, taskId);
            taskIdToResultsItem[taskId] = queue.currentResultsItem;
        }
    };

    agent->Console->SetNextCommandCallbacks(submitCb, taskIdCb);

    // Process command through commander AFTER callbacks are armed.
    // This is required so pre-hook implementations that call ax.execute_alias()
    // can still be tracked by Tactical Guidance.
    CommanderResult cmdResult = agent->commander->ProcessInput(agentId, commandLine);

    TG_DEBUG << "[TG] Commander result for agent" << agentId << "command:" << commandLine;
    TG_DEBUG << "[TG]   is_pre_hook:" << cmdResult.is_pre_hook << "output:" << cmdResult.output << "error:" << cmdResult.error;
    TG_DEBUG << "[TG]   cmdResult.data:" << QJsonDocument(cmdResult.data).toJson();

    if (cmdResult.output) {
        if (agentResItem) {
            agentResItem->setText(3, cmdResult.error ? "Error" : "Success");
        }

        AgentExecutionQueue& queue = agentQueues[agentId];
        queue.isWaitingForTask = false;
        queue.currentTaskId.clear();
        queue.currentCommand = nullptr;

        if (cmdResult.error && chkStopOnError && chkStopOnError->isChecked()) {
            stopExecution();
        } else {
            QTimer::singleShot(0, this, [this, agentId]() {
                if (executionRunning)
                    executeNextCommandForAgent(agentId);
            });
            finalizeExecutionIfDone();
        }
        return;
    }

    if (cmdResult.is_pre_hook)
        return;

    if (agentResItem)
        agentResItem->setText(3, "Submitted");

    agent->Console->ProcessCmdResult(commandLine, cmdResult, false);
}

void TacticalGuidanceWidget::notifyTaskSuccess(const QString& taskId)
{
    if (!executionRunning) {
        TG_DEBUG << "[TG] notifyTaskSuccess: execution not running, ignoring:" << taskId;
        return;
    }

    // Find the agent queue that contains this task
    QString agentId;
    for (auto it = agentQueues.begin(); it != agentQueues.end(); ++it) {
        if (it.value().currentTaskId == taskId) {
            agentId = it.key();
            break;
        }
    }

    if (agentId.isEmpty()) {
        return;
    }

    QTreeWidgetItem* resItem = taskIdToResultsItem.value(taskId, nullptr);
    if (resItem)
        resItem->setText(3, "Success");

    AgentExecutionQueue& queue = agentQueues[agentId];
    
    TG_DEBUG << "[TG] notifyTaskSuccess: Task" << taskId << "for agent" << agentId << "completed successfully";
    
    // Mark current task as completed
    queue.isWaitingForTask = false;
    queue.currentTaskId.clear();
    queue.currentCommand = nullptr;

    // Execute next command for this agent
    executeNextCommandForAgent(agentId);
    finalizeExecutionIfDone();
}

void TacticalGuidanceWidget::handleTaskUpdate(const TaskData& task)
{
    // L4 + A mode: task-result binding/execution gating is disabled.

    if (!executionRunning)
        return;

    const QString taskId = task.TaskId;
    if (taskId.isEmpty())
        return;

    QTreeWidgetItem* resItemByTask = taskIdToResultsItem.value(taskId, nullptr);
    if (resItemByTask)
        resItemByTask->setText(3, task.Status);

    if (!taskIdToComposerItem.contains(taskId)) {
        if (!task.Completed)
            return;
        const QString finalStatus = adaptixWidget && adaptixWidget->TasksMap.contains(taskId)
            ? adaptixWidget->TasksMap[taskId].Status
            : task.Status;
        if (resItemByTask)
            resItemByTask->setText(3, finalStatus);
        return;
    }

    QTreeWidgetItem* stepItem = taskIdToComposerItem.value(taskId, nullptr);
    if (!stepItem)
        return;

    const QString stepInstanceId = stepItem->data(0, Qt::UserRole).toString();
    const QString agentKey = stepInstanceId + "|" + task.AgentId;
    QTreeWidgetItem* agentResItem = resultsAgentItems.value(agentKey, nullptr);
    if (agentResItem) {
        agentResItem->setText(3, task.Status);  // Status column
    }

    // Debug: Log task state changes
    TG_DEBUG << "[TG] TaskUpdate:" << taskId << "Agent:" << task.AgentId 
             << "Status:" << task.Status << "Completed:" << task.Completed;

    // Only process completion when task is actually completed
    if (!task.Completed) {
        TG_DEBUG << "[TG] Task not completed yet, waiting:" << taskId;
        return;
    }

    // CRITICAL: Verify the task status matches what's displayed in TasksWidget
    // This ensures we only advance when the UI actually shows "Success"
    QString finalStatus = task.Status;
    if (adaptixWidget && adaptixWidget->TasksMap.contains(taskId)) {
        const TaskData& uiTask = adaptixWidget->TasksMap[taskId];
        TG_DEBUG << "[TG] UI Task Status:" << uiTask.Status << "vs Task Status:" << task.Status;
        finalStatus = uiTask.Status;
    }
    if (agentResItem) {
        agentResItem->setText(3, finalStatus);
    }

    QTreeWidgetItem* resItemFallback = taskIdToResultsItem.value(taskId, nullptr);
    if (resItemFallback)
        resItemFallback->setText(3, finalStatus);

    // Success path: advance normally (idempotent even if TasksWidget also calls notifyTaskSuccess)
    if (finalStatus == "Success") {
        notifyTaskSuccess(taskId);
        return;
    }

    // Non-success completion: clear the agent queue's waiting state, otherwise last step can stay "Running".
    if (agentQueues.contains(task.AgentId)) {
        AgentExecutionQueue& queue = agentQueues[task.AgentId];
        if (queue.currentTaskId == taskId) {
            queue.isWaitingForTask = false;
            queue.currentTaskId.clear();
            queue.currentCommand = nullptr;
        }
    }

    if (chkStopOnError && chkStopOnError->isChecked()) {
        stopExecution();
        return;
    }

    // Continue with next step for this agent
    QTimer::singleShot(0, this, [this, agentId = task.AgentId]() {
        if (executionRunning)
            executeNextCommandForAgent(agentId);
    });
    finalizeExecutionIfDone();
}

void TacticalGuidanceWidget::addStepToActivePlaybook(const QString& commandId, const QMap<QString, QString>& params)
{
    if (!commandMap.contains(commandId))
        return;

    QTreeWidgetItem* selected = composerTree ? composerTree->currentItem() : nullptr;
    QTreeWidgetItem* parent = nullptr;
    int insertIndex = -1;

    if (selected) {
        const QString selType = selected->data(4, Qt::UserRole).toString();
        if (selType == "group") {
            parent = selected;
            insertIndex = parent->childCount();
        } else {
            parent = selected->parent();
            if (parent)
                insertIndex = parent->indexOfChild(selected) + 1;
            else if (composerTree)
                insertIndex = composerTree->indexOfTopLevelItem(selected) + 1;
        }
    }

    QTreeWidgetItem* item = nullptr;
    if (parent) {
        item = new QTreeWidgetItem();
        parent->insertChild(insertIndex, item);
        parent->setExpanded(true);
    } else if (composerTree) {
        item = new QTreeWidgetItem();
        composerTree->insertTopLevelItem(insertIndex < 0 ? composerTree->topLevelItemCount() : insertIndex, item);
    }

    if (!item)
        return;

    const auto& cmd = commandMap[commandId];

    item->setText(0, cmd.name);
    item->setText(1, cmd.description);
    item->setText(2, cmd.rawCommand);

    item->setData(0, Qt::UserRole, QUuid::createUuid().toString());
    item->setData(2, Qt::UserRole, commandId);
    item->setData(4, Qt::UserRole, "command");

    QVariantMap paramsVar;
    for (auto it = params.begin(); it != params.end(); ++it)
        paramsVar.insert(it.key(), it.value());
    item->setData(3, Qt::UserRole, paramsVar);

    item->setData(5, Qt::UserRole, QVariantMap());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);

    if (cmd.os == 1) item->setIcon(0, QIcon(":/icons/os_win_blue"));
    else if (cmd.os == 2) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
    else if (cmd.os == 3) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
    else item->setIcon(0, QIcon(":/icons/code_blocks"));
}

void TacticalGuidanceWidget::clearWorkflow()
{
    stopExecution();
    composerTree->clear();
    if (resultsTree)
        resultsTree->clear();
}

QJsonObject TacticalGuidanceWidget::getLibraryAsJson() const
{
    QJsonObject root;
    QJsonArray nodesArr;

    std::function<QJsonObject(QStandardItem*)> serializeItem = [&](QStandardItem* item) -> QJsonObject {
        QJsonObject obj;
        QString id = item->data(Qt::UserRole).toString();
        QString type = item->data(Qt::UserRole + 1).toString();
        
        if (id.isEmpty()) return QJsonObject(); // Should not happen for valid nodes

        if (nodeMap.contains(id)) {
            const auto& nodeData = nodeMap[id];
            obj["id"] = nodeData.id;
            obj["name"] = nodeData.name;
            obj["type"] = nodeData.type;
            obj["description"] = nodeData.description;
            obj["parent_id"] = nodeData.parentId;
            
            if (nodeData.type == "command") {
                QJsonObject cmdObj;
                const auto& cmd = nodeData.command;
                cmdObj["id"] = cmd.id;
                cmdObj["name"] = cmd.name;
                cmdObj["cmd"] = cmd.rawCommand;
                cmdObj["os"] = cmd.os;
                cmdObj["risk"] = cmd.risk;
                cmdObj["description"] = cmd.description;
                obj["command"] = cmdObj;
            }
        } else {
            // Fallback if not in map (e.g. newly created via context menu but map update missed?)
            // We ensure map is updated in context menu.
            obj["id"] = id;
            obj["name"] = item->text();
            obj["type"] = type.isEmpty() ? "category" : type; 
        }

        QJsonArray children;
        for (int i = 0; i < item->rowCount(); ++i) {
            children.append(serializeItem(item->child(i)));
        }
        if (!children.isEmpty()) obj["children"] = children;

        return obj;
    };

    for (int i = 0; i < libraryModel->rowCount(); ++i) {
        nodesArr.append(serializeItem(libraryModel->item(i)));
    }
    
    root["nodes"] = nodesArr;
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

void TacticalGuidanceWidget::onLibrarySearchChanged(const QString& text)
{
    if (libraryProxyModel)
        libraryProxyModel->setFilterWildcard(text);
}

void TacticalGuidanceWidget::cleanupNodeData(QStandardItem* item)
{
    if (!item) return;
    
    // Recursive clean children first
    for (int i = 0; i < item->rowCount(); ++i) {
        cleanupNodeData(item->child(i));
    }

    QString id = item->data(Qt::UserRole).toString();
    nodeMap.remove(id);
    commandMap.remove(id);
}

QStandardItem* TacticalGuidanceWidget::findItemById(const QString& id, QStandardItem* parent)
{
    if (!libraryModel) return nullptr;
    
    QStandardItem* root = parent ? parent : libraryModel->invisibleRootItem();
    
    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i);
        if (child->data(Qt::UserRole).toString() == id) {
            return child;
        }
        
        QStandardItem* found = findItemById(id, child);
        if (found) return found;
    }
    return nullptr;
}

QMap<QString, QString> TacticalGuidanceWidget::getAllCategories(QStandardItem* parent, QString prefix)
{
    QMap<QString, QString> categories;
    QStandardItem* root = parent ? parent : libraryModel->invisibleRootItem();
    
    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i);
        QString type = child->data(Qt::UserRole + 1).toString();
        
        if (type == "category") {
            QString id = child->data(Qt::UserRole).toString();
            QString name = child->text();
            categories[id] = prefix + name;
            
            // Recurse
            QMap<QString, QString> subs = getAllCategories(child, prefix + name + " / ");
            for(auto it = subs.constBegin(); it != subs.constEnd(); ++it) {
                categories.insert(it.key(), it.value());
            }
        }
    }
    return categories;
}

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QPlainTextEdit>

bool TacticalGuidanceWidget::showCommandDialog(TacticalCommandData& data, QString& parentId, const QString& title)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.resize(500, 500);
    
    QFormLayout* layout = new QFormLayout(&dlg);
    
    // Parent Selection
    QComboBox* comboParent = new QComboBox(&dlg);
    QMap<QString, QString> cats = getAllCategories();
    
    int currentIndex = 0;
    int i = 0;
    for (auto it = cats.begin(); it != cats.end(); ++it) {
        comboParent->addItem(it.value(), it.key());
        if (it.key() == parentId) currentIndex = i;
        i++;
    }
    layout->addRow("Location:", comboParent);
    comboParent->setCurrentIndex(currentIndex);

    QLineEdit* editName = new QLineEdit(&dlg);
    editName->setText(data.name);
    layout->addRow("Name:", editName);
    
    QPlainTextEdit* editCmd = new QPlainTextEdit(&dlg);
    editCmd->setPlainText(data.rawCommand);
    editCmd->setFixedHeight(80);
    layout->addRow("Command:", editCmd);
    
    QLineEdit* editDesc = new QLineEdit(&dlg);
    editDesc->setText(data.description);
    layout->addRow("Description:", editDesc);
    
    QLineEdit* editUsage = new QLineEdit(&dlg);
    editUsage->setText(data.usage);
    editUsage->setPlaceholderText("Optional usage info");
    layout->addRow("Usage:", editUsage);
    
    QComboBox* comboOs = new QComboBox(&dlg);
    comboOs->addItems({"Any", "Windows", "Linux", "macOS"});
    // Map 1=Win, 2=Linux, 3=Mac. 0=Any? Assuming 0 is Any for now or just default to 1
    int osIdx = 0;
    if (data.os == 1) osIdx = 1;
    else if (data.os == 2) osIdx = 2;
    else if (data.os == 3) osIdx = 3;
    comboOs->setCurrentIndex(osIdx);
    layout->addRow("OS:", comboOs);
    
    QComboBox* comboArch = new QComboBox(&dlg);
    comboArch->addItems({"Any", "x86", "x64"});
    int archIdx = 0;
    if (data.arch == 1) archIdx = 1;
    else if (data.arch == 2) archIdx = 2;
    comboArch->setCurrentIndex(archIdx);
    layout->addRow("Architecture:", comboArch);
    
    QComboBox* comboRisk = new QComboBox(&dlg);
    comboRisk->addItems({"Low", "Medium", "High"});
    // 1=Low, 2=Med, 3=High
    int riskIdx = 0;
    if (data.risk >= 1 && data.risk <= 3) riskIdx = data.risk - 1;
    comboRisk->setCurrentIndex(riskIdx);
    layout->addRow("Risk Level:", comboRisk);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addRow(buttons);
    
    if (dlg.exec() == QDialog::Accepted) {
        parentId = comboParent->currentData().toString();
        data.name = editName->text();
        data.rawCommand = editCmd->toPlainText();
        data.description = editDesc->text();
        data.usage = editUsage->text();
        
        // OS
        int sOs = comboOs->currentIndex();
        if (sOs == 1) data.os = 1;
        else if (sOs == 2) data.os = 2;
        else if (sOs == 3) data.os = 3;
        else data.os = 0; // Any
        
        // Arch
        int sArch = comboArch->currentIndex();
        if (sArch == 1) data.arch = 1;
        else if (sArch == 2) data.arch = 2;
        else data.arch = 0;
        
        // Risk
        data.risk = comboRisk->currentIndex() + 1;
        
        return true;
    }
    return false;
}

void TacticalGuidanceWidget::onLibraryContextMenu(const QPoint& pos)
{
    QModelIndex index = libraryView->indexAt(pos);
    QMenu menu(this);

    if (!index.isValid()) {
        menu.addAction("Add Category", this, [this](){
            bool ok;
            QString text = QInputDialog::getText(this, "Add Category", "Category Name:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                QString id = QUuid::createUuid().toString();
                TacticalNodeData node;
                node.id = id;
                node.name = text;
                node.type = "category";
                node.description = "";
                nodeMap[id] = node;

                auto* item = new QStandardItem(text);
                item->setData(id, ROLE_NODE_ID);
                item->setData("category", ROLE_NODE_TYPE);
                item->setFont(QFont("", -1, QFont::Bold));
                item->setSelectable(false);
                item->setIcon(QIcon(":/icons/folder"));
                libraryModel->appendRow(item);
                
                saveLibrary();
            }
        });
    } else {
        auto sourceIndex = libraryProxyModel->mapToSource(index);
        auto* item = libraryModel->itemFromIndex(sourceIndex);
        
        QString id = item->data(ROLE_NODE_ID).toString();
        QString type = item->data(ROLE_NODE_TYPE).toString();

        if (type == "category") {
            menu.addAction("Add Subcategory", this, [this, item, id](){
                bool ok;
                QString text = QInputDialog::getText(this, "Add Subcategory", "Name:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) {
                    QString newId = QUuid::createUuid().toString();
                    TacticalNodeData node;
                    node.id = newId;
                    node.parentId = id;
                    node.name = text;
                    node.type = "category";
                    nodeMap[newId] = node;

                    auto* newItem = new QStandardItem(text);
                    newItem->setData(newId, ROLE_NODE_ID);
                    newItem->setData("category", ROLE_NODE_TYPE);
                    newItem->setFont(QFont("", -1, QFont::Bold));
                    newItem->setSelectable(false);
                    newItem->setIcon(QIcon(":/icons/folder"));
                    item->appendRow(newItem);
                    libraryView->expand(libraryProxyModel->mapFromSource(item->index()));
                    
                    saveLibrary();
                }
            });

            menu.addAction("Add Command", this, [this, item, id](){
                TacticalCommandData cmd;
                cmd.os = 1; // Default Win
                cmd.risk = 1;
                QString pId = id;
                
                if (showCommandDialog(cmd, pId, "Add Command")) {
                    QString newId = QUuid::createUuid().toString();
                    cmd.id = newId;
                    
                    TacticalNodeData node;
                    node.id = newId;
                    node.parentId = pId;
                    node.name = cmd.name;
                    node.type = "command";
                    node.description = cmd.description;
                    node.command = cmd;
                    
                    nodeMap[newId] = node;
                    commandMap[newId] = cmd;

                    auto* newItem = new QStandardItem(cmd.name);
                    newItem->setData(newId, ROLE_NODE_ID);
                    newItem->setData("command", ROLE_NODE_TYPE);
                    newItem->setData(cmd.os, ROLE_COMMAND_OS);
                    newItem->setToolTip(QString("%1\n\nCommand: %2").arg(cmd.description).arg(cmd.rawCommand));
                    
                    if (cmd.os == 1) newItem->setIcon(QIcon(":/icons/os_win_blue"));
                    else if (cmd.os == 2) newItem->setIcon(QIcon(":/icons/os_linux_blue"));
                    else if (cmd.os == 3) newItem->setIcon(QIcon(":/icons/os_mac_blue"));
                    else newItem->setIcon(QIcon(":/icons/code_blocks")); // Fallback
                    
                    // If pId changed from current item id, we need to find that parent
                    QStandardItem* targetParent = item;
                    if (pId != id) {
                         QStandardItem* found = findItemById(pId);
                         if (found) targetParent = found;
                    }
                    targetParent->appendRow(newItem);
                    libraryView->expand(libraryProxyModel->mapFromSource(targetParent->index()));
                    
                    saveLibrary();
                }
            });
            
            menu.addSeparator();
            
            menu.addAction("Rename", this, [this, item, id](){
                bool ok;
                QString text = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, item->text(), &ok);
                if (ok && !text.isEmpty()) {
                    item->setText(text);
                    if (nodeMap.contains(id)) nodeMap[id].name = text;
                    saveLibrary();
                }
            });

            menu.addAction("Delete Category", this, [this, item, id](){
                if (QMessageBox::question(this, "Delete", "Are you sure you want to delete this category and ALL contents?") == QMessageBox::Yes) {
                    cleanupNodeData(item);
                    if (item->parent()) item->parent()->removeRow(item->row());
                    else libraryModel->removeRow(item->row());
                    saveLibrary();
                }
            });
        } 
        else if (type == "command") {
            menu.addAction("Edit", this, [this, item, id](){
                if (commandMap.contains(id)) {
                    TacticalCommandData cmd = commandMap[id];
                    QString pId = nodeMap[id].parentId;
                    QString oldPid = pId;
                    
                    if (showCommandDialog(cmd, pId, "Edit Command")) {
                        // Update Data
                        commandMap[id] = cmd;
                        if (nodeMap.contains(id)) {
                            nodeMap[id].command = cmd;
                            nodeMap[id].name = cmd.name;
                            nodeMap[id].description = cmd.description;
                            nodeMap[id].parentId = pId;
                        }
                        
                        // Update UI
                        item->setText(cmd.name);
                        item->setToolTip(QString("%1\n\nCommand: %2").arg(cmd.description).arg(cmd.rawCommand));
                        item->setData(cmd.os, ROLE_COMMAND_OS);
                        
                        if (cmd.os == 1) item->setIcon(QIcon(":/icons/os_win_blue"));
                        else if (cmd.os == 2) item->setIcon(QIcon(":/icons/os_linux_blue"));
                        else if (cmd.os == 3) item->setIcon(QIcon(":/icons/os_mac_blue"));
                        else item->setIcon(QIcon(":/icons/code_blocks"));
                        
                        // Handle Move if parent changed
                        if (pId != oldPid) {
                             QStandardItem* newParent = findItemById(pId);
                             if (newParent) {
                                 // Take from old, add to new
                                 QList<QStandardItem*> row;
                                 if (item->parent()) {
                                     row = item->parent()->takeRow(item->row());
                                 } else {
                                     row = libraryModel->takeRow(item->row());
                                 }
                                 newParent->appendRow(row);
                                 libraryView->expand(libraryProxyModel->mapFromSource(newParent->index()));
                             }
                        }
                        
                        saveLibrary();
                    }
                }
            });
            
            menu.addAction("Rename", this, [this, item, id](){
                bool ok;
                QString text = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, item->text(), &ok);
                if (ok && !text.isEmpty()) {
                    item->setText(text);
                    if (nodeMap[id].type == "command") {
                        nodeMap[id].command.name = text;
                        commandMap[id].name = text;
                    }
                    saveLibrary();
                }
            });
            
            menu.addAction("Delete Command", this, [this, item, id](){
                if (QMessageBox::question(this, "Delete", "Delete this command?") == QMessageBox::Yes) {
                    cleanupNodeData(item);
                    if (item->parent()) item->parent()->removeRow(item->row());
                    else libraryModel->removeRow(item->row());
                    saveLibrary();
                }
            });
        }
    }

    menu.exec(libraryView->viewport()->mapToGlobal(pos));
}

QJsonObject TacticalGuidanceWidget::serializeNode(QStandardItem* item)
{
    // Deprecated. Use serializeLibraryTree instead.
    return QJsonObject();
}

QJsonArray TacticalGuidanceWidget::serializeLibraryTree(QStandardItem* parent)
{
    QJsonArray arr;
    QStandardItem* root = parent ? parent : libraryModel->invisibleRootItem();

    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i);
        QString id = child->data(Qt::UserRole).toString();
        
        // Ensure data consistency
        if (!nodeMap.contains(id)) continue;
        
        QJsonObject obj;
        TacticalNodeData& nodeData = nodeMap[id];
        
        obj["id"] = nodeData.id;
        obj["name"] = nodeData.name;
        obj["type"] = nodeData.type;
        obj["description"] = nodeData.description;
        obj["parent_id"] = nodeData.parentId;
        
        if (nodeData.type == "command") {
            QJsonObject cmdObj;
            const auto& cmd = nodeData.command;
            cmdObj["id"] = cmd.id;
            cmdObj["name"] = cmd.name;
            cmdObj["cmd"] = cmd.rawCommand;
            cmdObj["os"] = cmd.os;
            cmdObj["risk"] = cmd.risk;
            cmdObj["description"] = cmd.description;
            cmdObj["arch"] = cmd.arch;
            cmdObj["usage"] = cmd.usage;
            obj["command"] = cmdObj;
        } else if (nodeData.type == "category") {
            // Recursively serialize children
            obj["children"] = serializeLibraryTree(child);
        }
        
        arr.append(obj);
    }
    return arr;
}

void TacticalGuidanceWidget::saveLibrary()
{
    QJsonArray rootArr = serializeLibraryTree(nullptr);
    QJsonDocument doc(rootArr);

    QDir dir(QDir::home().filePath(".adaptix"));
    if (!dir.exists("data"))
        dir.mkpath("data");

    QFile file(dir.absoluteFilePath("data/tactical_library.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    } else {
        TG_DEBUG << "Failed to save local tactical library:" << file.errorString();
    }
}

void TacticalGuidanceWidget::rebuildLibraryFromJSON(const QJsonArray& nodes)
{
    TG_DEBUG << "[Tactical] Rebuilding library from JSON with" << nodes.size() << "root nodes.";
    // REMOVED QSignalBlocker to ensure ProxyModel updates correctly
    libraryModel->clear();
    nodeMap.clear();
    commandMap.clear();
    
    std::function<void(QStandardItem*, const QJsonArray&)> build;
    build = [&](QStandardItem* parent, const QJsonArray& list) {
        for(const auto& val : list) {
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString type = obj["type"].toString();
            QString name = obj["name"].toString();
            
            // Debug check
            // qDebug() << "[Tactical] Processing node:" << name << type << id;

            TacticalNodeData node;
            node.id = id;
            node.name = name;
            node.type = type;
            node.description = obj["description"].toString();
            node.parentId = obj["parent_id"].toString();
            
            nodeMap[id] = node;
            
            auto* item = new QStandardItem(name);
            item->setData(id, ROLE_NODE_ID);
            item->setData(type, ROLE_NODE_TYPE);
            
            if (type == "category") {
                item->setIcon(QIcon(":/icons/folder"));
                item->setFont(QFont("", -1, QFont::Bold));
                item->setSelectable(false);
                if (obj.contains("children")) build(item, obj["children"].toArray());
            } else if (type == "command") {
                QJsonObject cmdObj = obj["command"].toObject();
                node.command.id = cmdObj["id"].toString();
                node.command.name = cmdObj["name"].toString();
                node.command.rawCommand = cmdObj["cmd"].toString();
                node.command.os = cmdObj["os"].toInt();
                node.command.risk = cmdObj["risk"].toInt();
                node.command.description = cmdObj["description"].toString();
                node.command.usage = cmdObj["usage"].toString();
                node.command.arch = cmdObj["arch"].toInt();
                
                commandMap[id] = node.command;
                nodeMap[id].command = node.command;
                
                int os = node.command.os;
                item->setData(os, ROLE_COMMAND_OS);
                if (os == 1) item->setIcon(QIcon(":/icons/os_win_blue"));
                else if (os == 2) item->setIcon(QIcon(":/icons/os_linux_blue"));
                else if (os == 3) item->setIcon(QIcon(":/icons/os_mac_blue"));
                else item->setIcon(QIcon(":/icons/code_blocks"));
                
                item->setToolTip(node.description + "\n\n" + node.command.rawCommand);
            }
            
            if (parent) parent->appendRow(item);
            else libraryModel->appendRow(item);
        }
    };
    
    build(nullptr, nodes);
    if (libraryView) libraryView->expandAll();
    qDebug() << "[Tactical] Library rebuild complete. Total items in map:" << nodeMap.size();
}

void TacticalGuidanceWidget::loadLibrary()
{
    QDir dir(QDir::home().filePath(".adaptix"));
    if (!dir.exists("data"))
        dir.mkpath("data");

    const QString newPath = dir.absoluteFilePath("data/tactical_library.json");

    QDir oldDir(QCoreApplication::applicationDirPath());
    const QString oldPath = oldDir.absoluteFilePath("data/tactical_library.json");

    if (!QFile::exists(newPath) && QFile::exists(oldPath)) {
        if (!QFile::rename(oldPath, newPath)) {
            QFile::copy(oldPath, newPath);
            QFile::remove(oldPath);
        }
    }

    if (!QFile::exists(newPath)) {
        QFile res(":/tactical/default_library");
        if (res.open(QIODevice::ReadOnly)) {
            QFile out(newPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(res.readAll());
                out.close();
            }
            res.close();
        }
    }

    QString path = newPath;
    qDebug() << "[Tactical] Loading library from:" << path;
    
    QFile file(path);
    
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            rebuildLibraryFromJSON(doc.array());
        } else {
            qDebug() << "[Tactical] Invalid JSON or not an array.";
        }
        file.close();
    } else {
        qDebug() << "[Tactical] Library file not found or cannot open.";
    }
}

void TacticalGuidanceWidget::ensureDefaultPlaybook()
{
    if (playbooks.isEmpty()) {
        QJsonObject pb;
        pb["id"] = QUuid::createUuid().toString();
        pb["name"] = "Active";
        pb["nodes"] = QJsonArray();
        playbooks.append(pb);
    }

    if (currentPlaybookId.isEmpty())
        currentPlaybookId = playbooks.first().toObject()["id"].toString();

    bool found = false;
    for (const auto& v : playbooks) {
        if (v.toObject()["id"].toString() == currentPlaybookId) {
            found = true;
            break;
        }
    }

    if (!found)
        currentPlaybookId = playbooks.first().toObject()["id"].toString();
}

void TacticalGuidanceWidget::rebuildPlaybookList()
{
    if (!playbookSelector)
        return;

    ensureDefaultPlaybook();

    QSignalBlocker blocker(playbookSelector);
    playbookSelector->clear();

    int idx = 0;
    int currentIdx = 0;
    for (const auto& v : playbooks) {
        QJsonObject pb = v.toObject();
        const QString id = pb["id"].toString();
        const QString name = pb["name"].toString();
        playbookSelector->addItem(name, id);
        if (id == currentPlaybookId)
            currentIdx = idx;
        idx++;
    }
    playbookSelector->setCurrentIndex(currentIdx);
}

QJsonObject TacticalGuidanceWidget::serializeComposerItem(QTreeWidgetItem* item) const
{
    QJsonObject obj;
    if (!item)
        return obj;

    const QString type = item->data(4, Qt::UserRole).toString();
    obj["type"] = type;
    obj["name"] = item->text(0);
    obj["instance_id"] = item->data(0, Qt::UserRole).toString();

    if (type == "group") {
        QJsonArray children;
        for (int i = 0; i < item->childCount(); ++i) {
            QJsonObject c = serializeComposerItem(item->child(i));
            if (!c.isEmpty())
                children.append(c);
        }
        obj["children"] = children;
    } else if (type == "command") {
        obj["command_id"] = item->data(2, Qt::UserRole).toString();
        obj["raw_command"] = item->text(2);
        const QVariantMap params = item->data(3, Qt::UserRole).toMap();
        if (!params.isEmpty())
            obj["params"] = QJsonObject::fromVariantMap(params);
    }

    return obj;
}

QTreeWidgetItem* TacticalGuidanceWidget::deserializeComposerItem(const QJsonObject& obj, QTreeWidgetItem* parent)
{
    if (!composerTree)
        return nullptr;

    const QString type = obj["type"].toString();
    const QString name = obj["name"].toString();
    const QString instanceId = obj["instance_id"].toString();

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setText(1, "");
    item->setText(2, "");
    item->setData(0, Qt::UserRole, instanceId.isEmpty() ? QUuid::createUuid().toString() : instanceId);
    item->setData(4, Qt::UserRole, type);

    if (type == "group") {
        item->setIcon(0, QIcon(":/icons/folder"));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        if (parent)
            parent->addChild(item);
        else
            composerTree->addTopLevelItem(item);
        item->setExpanded(true);

        const QJsonArray children = obj["children"].toArray();
        for (const auto& v : children) {
            deserializeComposerItem(v.toObject(), item);
        }
        return item;
    }

    if (type == "command") {
        const QString commandId = obj["command_id"].toString();
        item->setData(2, Qt::UserRole, commandId);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);

        if (obj.contains("params") && obj["params"].isObject()) {
            const QVariantMap params = obj["params"].toObject().toVariantMap();
            item->setData(3, Qt::UserRole, params);
        } else {
            item->setData(3, Qt::UserRole, QVariantMap());
        }
        item->setData(5, Qt::UserRole, QVariantMap());

        if (commandMap.contains(commandId)) {
            const auto& cmd = commandMap[commandId];
            item->setText(0, cmd.name);
            item->setText(1, cmd.description);
            const QString raw = obj.contains("raw_command") ? obj["raw_command"].toString() : QString();
            item->setText(2, raw.isEmpty() ? cmd.rawCommand : raw);

            if (cmd.os == 1) item->setIcon(0, QIcon(":/icons/os_win_blue"));
            else if (cmd.os == 2) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
            else if (cmd.os == 3) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
            else item->setIcon(0, QIcon(":/icons/code_blocks"));
        }

        if (parent)
            parent->addChild(item);
        else
            composerTree->addTopLevelItem(item);
        return item;
    }

    delete item;
    return nullptr;
}

void TacticalGuidanceWidget::storeCurrentPlaybook()
{
    if (currentPlaybookId.isEmpty() || !composerTree)
        return;

    QJsonArray nodes;
    for (int i = 0; i < composerTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* it = composerTree->topLevelItem(i);
        QJsonObject obj = serializeComposerItem(it);
        if (!obj.isEmpty())
            nodes.append(obj);
    }

    QJsonArray newArr;
    for (const auto& v : playbooks) {
        QJsonObject pb = v.toObject();
        if (pb["id"].toString() == currentPlaybookId)
            pb["nodes"] = nodes;
        newArr.append(pb);
    }
    playbooks = newArr;
}

void TacticalGuidanceWidget::switchPlaybook(const QString& playbookId)
{
    if (playbookId.isEmpty())
        return;

    storeCurrentPlaybook();
    currentPlaybookId = playbookId;

    rebuildPlaybookList();

    if (!composerTree)
        return;

    composerIsLoading = true;
    composerTree->clear();

    QJsonArray nodes;
    for (const auto& v : playbooks) {
        QJsonObject pb = v.toObject();
        if (pb["id"].toString() == currentPlaybookId) {
            nodes = pb["nodes"].toArray();
            break;
        }
    }
    for (const auto& v : nodes)
        deserializeComposerItem(v.toObject(), nullptr);

    composerIsLoading = false;
}

void TacticalGuidanceWidget::saveComposer()
{
    ensureDefaultPlaybook();
    storeCurrentPlaybook();

    QJsonObject root;
    root["version"] = 1;
    root["current_playbook_id"] = currentPlaybookId;
    root["playbooks"] = playbooks;

    QJsonDocument doc(root);

    QDir dir(QDir::home().filePath(".adaptix"));
    if (!dir.exists("data"))
        dir.mkpath("data");

    QFile file(dir.absoluteFilePath("data/tactical_composer.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qDebug() << "Failed to save local tactical composer:" << file.errorString();
    }
}

void TacticalGuidanceWidget::loadComposer()
{
    if (!composerTree)
        return;

    composerIsLoading = true;
    playbooks = QJsonArray();
    currentPlaybookId.clear();

    QDir dir(QDir::home().filePath(".adaptix"));
    if (!dir.exists("data"))
        dir.mkpath("data");

    const QString newPath = dir.absoluteFilePath("data/tactical_composer.json");

    QDir oldDir(QCoreApplication::applicationDirPath());
    const QString oldPath = oldDir.absoluteFilePath("data/tactical_composer.json");

    if (!QFile::exists(newPath) && QFile::exists(oldPath)) {
        if (!QFile::rename(oldPath, newPath)) {
            QFile::copy(oldPath, newPath);
            QFile::remove(oldPath);
        }
    }

    QFile file(newPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            currentPlaybookId = root["current_playbook_id"].toString();
            if (root.contains("playbooks") && root["playbooks"].isArray())
                playbooks = root["playbooks"].toArray();
        }
        file.close();
    }

    ensureDefaultPlaybook();
    rebuildPlaybookList();

    composerTree->clear();
    QJsonArray nodes;
    for (const auto& v : playbooks) {
        QJsonObject pb = v.toObject();
        if (pb["id"].toString() == currentPlaybookId) {
            nodes = pb["nodes"].toArray();
            break;
        }
    }
    for (const auto& v : nodes)
        deserializeComposerItem(v.toObject(), nullptr);

    composerIsLoading = false;
}

bool TacticalGuidanceWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (!composerTree || !libraryView)
        return DockTab::eventFilter(obj, event);

    enum class DropPos {
        OnItem,
        AboveItem,
        BelowItem,
    };

    if (obj == libraryView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                auto index = libraryView->indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    auto sourceIndex = libraryProxyModel->mapToSource(index);
                    auto* item = libraryModel->itemFromIndex(sourceIndex);
                    
                    // Fix: Allow dragging any command regardless of depth
                    QString type = item->data(Qt::UserRole + 1).toString();
                    if (item && type == "command") {
                        QDrag* drag = new QDrag(this);
                        QMimeData* mimeData = new QMimeData;
                        
                        QString commandId = item->data(Qt::UserRole).toString();
                        mimeData->setData(MIME_TACTICAL_BLOCK, commandId.toUtf8());
                        
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
            if (dragEvent->source() == composerTree) {
                 // Pass to QTreeWidget for indicator setup
                 return false; 
            }
        } else if (event->type() == QEvent::DragMove) {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (dragEvent->mimeData()->hasFormat(MIME_TACTICAL_BLOCK)) {
                dragEvent->acceptProposedAction();
                return true;
            }
            if (dragEvent->source() == composerTree) {
                 // Pass to QTreeWidget for indicator update
                 return false; 
            }
        } else if (event->type() == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            
            // Handle Library Drop
            if (dropEvent->mimeData()->hasFormat(MIME_TACTICAL_BLOCK)) {
                QByteArray data = dropEvent->mimeData()->data(MIME_TACTICAL_BLOCK);
                QString commandId = QString::fromUtf8(data);
                if (!commandMap.contains(commandId))
                    return true;

                QTreeWidgetItem* targetItem = composerTree->itemAt(dropEvent->position().toPoint());
                const QPoint pos = dropEvent->position().toPoint();
                DropPos indicator = DropPos::OnItem;
                if (targetItem) {
                    const QRect rect = composerTree->visualItemRect(targetItem);
                    const int y = pos.y();
                    const int third = qMax(1, rect.height() / 3);
                    if (y < rect.top() + third)
                        indicator = DropPos::AboveItem;
                    else if (y > rect.bottom() - third)
                        indicator = DropPos::BelowItem;
                    else
                        indicator = DropPos::OnItem;
                } else {
                    indicator = DropPos::BelowItem;
                }

                const auto& cmd = commandMap[commandId];
                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setText(0, cmd.name);
                item->setText(1, cmd.description);
                item->setText(2, cmd.rawCommand);
                item->setData(0, Qt::UserRole, QUuid::createUuid().toString());
                item->setData(2, Qt::UserRole, commandId);
                item->setData(4, Qt::UserRole, "command");
                item->setData(3, Qt::UserRole, QVariantMap());
                item->setData(5, Qt::UserRole, QVariantMap());
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
                if (cmd.os == 1) item->setIcon(0, QIcon(":/icons/os_win_blue"));
                else if (cmd.os == 2) item->setIcon(0, QIcon(":/icons/os_linux_blue"));
                else if (cmd.os == 3) item->setIcon(0, QIcon(":/icons/os_mac_blue"));
                else item->setIcon(0, QIcon(":/icons/code_blocks"));

                if (!targetItem) {
                    composerTree->addTopLevelItem(item);
                    dropEvent->acceptProposedAction();
                    return true;
                }

                if (indicator == DropPos::OnItem && targetItem->data(4, Qt::UserRole).toString() == "group") {
                    targetItem->addChild(item);
                    targetItem->setExpanded(true);
                    dropEvent->acceptProposedAction();
                    return true;
                }

                if (indicator == DropPos::OnItem) {
                    QTreeWidgetItem* p = targetItem->parent();
                    if (p) p->insertChild(p->indexOfChild(targetItem) + 1, item);
                    else composerTree->insertTopLevelItem(composerTree->indexOfTopLevelItem(targetItem) + 1, item);
                    dropEvent->acceptProposedAction();
                    return true;
                }

                QTreeWidgetItem* p = targetItem->parent();
                if (indicator == DropPos::AboveItem) {
                    if (p) p->insertChild(p->indexOfChild(targetItem), item);
                    else composerTree->insertTopLevelItem(composerTree->indexOfTopLevelItem(targetItem), item);
                } else if (indicator == DropPos::BelowItem) {
                    if (p) p->insertChild(p->indexOfChild(targetItem) + 1, item);
                    else composerTree->insertTopLevelItem(composerTree->indexOfTopLevelItem(targetItem) + 1, item);
                } else {
                    composerTree->addTopLevelItem(item);
                }

                dropEvent->acceptProposedAction();
                return true;
            }
            
            // Handle Internal Move
            if (dropEvent->source() == composerTree) {
                 // Let Qt handle internal move (InternalMove) to avoid item loss.
                 return false;
            }
        }
    }

    return DockTab::eventFilter(obj, event);
}

void TacticalGuidanceWidget::onAddGroupClicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, "Add Group", "Group Name:", QLineEdit::Normal, "", &ok);
    if (!ok || text.isEmpty()) return;

    QTreeWidgetItem* parentItem = nullptr;
    QList<QTreeWidgetItem*> selected = composerTree->selectedItems();
    if (!selected.isEmpty()) {
        // If a group is selected, add as child. If a command is selected, add to its parent.
        QTreeWidgetItem* sel = selected.first();
        if (sel->data(4, Qt::UserRole).toString() == "group") {
            parentItem = sel;
        } else {
            parentItem = sel->parent();
        }
    }

    QTreeWidgetItem* item = nullptr;
    if (parentItem) item = new QTreeWidgetItem(parentItem);
    else item = new QTreeWidgetItem(composerTree);

    item->setText(0, text);
    item->setText(1, "");
    item->setIcon(0, QIcon(":/icons/folder"));
    item->setData(0, Qt::UserRole, QUuid::createUuid().toString()); // instanceId
    item->setData(4, Qt::UserRole, "group"); // Type
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    
    item->setExpanded(true);
}

void TacticalGuidanceWidget::runActivePlaybook()
{
    const QList<QTreeWidgetItem*> steps = collectCommandSteps();
    startExecutionWithSteps(steps);
}

void TacticalGuidanceWidget::onComposerContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = composerTree->itemAt(pos);
    QMenu menu(this);

    if (item) {
        const QString type = item->data(4, Qt::UserRole).toString();
        const bool isCommand = (type == "command");

        QMenu* execMenu = menu.addMenu("Execute");
        QAction* actRunFromHere = execMenu->addAction("Run From Here");
        actRunFromHere->setEnabled(isCommand && !executionRunning);
        connect(actRunFromHere, &QAction::triggered, this, [this, item](){
            QList<QTreeWidgetItem*> all = collectCommandSteps();
            QList<QTreeWidgetItem*> tail;
            bool found = false;
            for (QTreeWidgetItem* it : all) {
                if (it == item)
                    found = true;
                if (found)
                    tail.push_back(it);
            }
            startExecutionWithSteps(tail);
        });

        if (!isCommand) {
            QAction* actDisabled = execMenu->addAction("Select a command step to execute");
            actDisabled->setEnabled(false);
        }

        menu.addSeparator();

        QMenu* editMenu = menu.addMenu("Edit");
        editMenu->addAction("Rename...", this, [this, item](){
            bool ok;
            QString text = QInputDialog::getText(this, "Rename", "Name:", QLineEdit::Normal, item->text(0), &ok);
            if (ok && !text.isEmpty()) {
                item->setText(0, text);
            }
        });

        editMenu->addSeparator();
        editMenu->addAction("Remove", this, [this, item](){
            delete item; // Removes from tree and parent
        });

        if (type == "group") {
            editMenu->addSeparator();
            editMenu->addAction("Add Group", this, &TacticalGuidanceWidget::onAddGroupClicked);
        }
    } else {
        QMenu* editMenu = menu.addMenu("Edit");
        editMenu->addAction("Add Group", this, &TacticalGuidanceWidget::onAddGroupClicked);
    }

    menu.exec(composerTree->viewport()->mapToGlobal(pos));
}

void TacticalGuidanceWidget::onComposerChanged()
{
    if (composerIsLoading)
        return;
    saveComposer();
}

void TacticalGuidanceWidget::onLibraryBlockSelected()
{
    auto index = libraryView->currentIndex();
    if (!index.isValid()) return;
}

void TacticalGuidanceWidget::onResultsItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item || !adaptixWidget)
        return;

    // Top-level agent row: open that agent's console.
    if (item->parent() == nullptr) {
        const QString agentId = item->text(0);
        if (!agentId.isEmpty())
            adaptixWidget->LoadConsoleUI(agentId);
        return;
    }

    const QString taskId = item->text(2);
    if (taskId.isEmpty())
        return;

    if (adaptixWidget->TasksDock)
        adaptixWidget->TasksDock->SelectTaskAndShowOutputById(taskId);
}

void TacticalGuidanceWidget::moveSelectedComposerItem(const int delta)
{
    if (!composerTree)
        return;
    if (delta == 0)
        return;

    QTreeWidgetItem* item = composerTree->currentItem();
    if (!item)
        return;

    QTreeWidgetItem* parent = item->parent();
    if (parent) {
        const int idx = parent->indexOfChild(item);
        const int newIdx = idx + delta;
        if (newIdx < 0 || newIdx >= parent->childCount())
            return;

        parent->takeChild(idx);
        parent->insertChild(newIdx, item);
        composerTree->setCurrentItem(item);
    } else {
        const int idx = composerTree->indexOfTopLevelItem(item);
        const int newIdx = idx + delta;
        if (newIdx < 0 || newIdx >= composerTree->topLevelItemCount())
            return;

        composerTree->takeTopLevelItem(idx);
        composerTree->insertTopLevelItem(newIdx, item);
        composerTree->setCurrentItem(item);
    }

    onComposerChanged();
}
