#include <UI/Dialogs/CommandPaletteDialog.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Agent/Agent.h>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>

CommandPaletteDialog::CommandPaletteDialog(AdaptixWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint), adaptixWidget(parent)
{
    createUI();
    loadTemplates();
    loadHistory();
    
    setFixedSize(600, 400);
    setStyleSheet(R"(
        CommandPaletteDialog {
            background-color: #1e1e1e;
            border: 1px solid #3c3c3c;
            border-radius: 8px;
        }
        QLineEdit {
            background-color: #2d2d2d;
            border: none;
            border-bottom: 1px solid #3c3c3c;
            color: #ffffff;
            font-size: 14px;
            padding: 12px;
        }
        QLineEdit:focus {
            border-bottom: 2px solid #0078d4;
        }
        QListWidget {
            background-color: transparent;
            border: none;
            color: #cccccc;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: 4px;
        }
        QListWidget::item:selected {
            background-color: #094771;
        }
        QListWidget::item:hover {
            background-color: #2a2d2e;
        }
        QLabel#preview {
            background-color: #252526;
            color: #9cdcfe;
            font-family: monospace;
            padding: 8px;
            border-top: 1px solid #3c3c3c;
        }
        QLabel#status {
            color: #6a6a6a;
            font-size: 11px;
            padding: 4px 12px;
        }
    )");
}

void CommandPaletteDialog::createUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("输入命令搜索...");
    searchEdit->installEventFilter(this);
    connect(searchEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::onSearchChanged);
    
    resultList = new QListWidget(this);
    resultList->setFocusPolicy(Qt::NoFocus);
    connect(resultList, &QListWidget::itemActivated, this, &CommandPaletteDialog::onItemActivated);
    connect(resultList, &QListWidget::itemSelectionChanged, this, &CommandPaletteDialog::onSelectionChanged);
    
    previewLabel = new QLabel(this);
    previewLabel->setObjectName("preview");
    previewLabel->setWordWrap(true);
    previewLabel->setMinimumHeight(40);
    previewLabel->setMaximumHeight(60);
    
    statusLabel = new QLabel(this);
    statusLabel->setObjectName("status");
    statusLabel->setText("↑↓ 导航  Enter 执行  Esc 关闭  Ctrl+C 复制");
    
    layout->addWidget(searchEdit);
    layout->addWidget(resultList, 1);
    layout->addWidget(previewLabel);
    layout->addWidget(statusLabel);
}

void CommandPaletteDialog::loadTemplates()
{
    allItems.clear();
    
    if (!adaptixWidget) return;
    
    // 从模板文件加载命令数据
    // 这里我们直接加载 YAML 文件
    QStringList templateFiles;
    templateFiles << ":/templates/recon_windows.yaml"
                  << ":/templates/recon_linux.yaml"
                  << ":/templates/privesc_windows.yaml"
                  << ":/templates/lateral_windows.yaml";
    
    QString localDir = QDir::homePath() + "/.adaptix/templates";
    QDir dir(localDir);
    if (dir.exists()) {
        for (const QFileInfo& fi : dir.entryInfoList(QStringList() << "*.yaml" << "*.yml", QDir::Files)) {
            if (!fi.fileName().startsWith("workflow")) {
                templateFiles << fi.absoluteFilePath();
            }
        }
    }
    
    for (const QString& filePath : templateFiles) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();
        
        QString category, currentOs;
        PaletteItem current;
        bool inTemplates = false;
        
        for (const QString& line : content.split('\n')) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
            
            if (trimmed.startsWith("category:")) {
                category = trimmed.mid(9).trimmed();
            } else if (trimmed.startsWith("os:") && !inTemplates) {
                currentOs = trimmed.mid(3).trimmed();
            } else if (trimmed == "templates:") {
                inTemplates = true;
            } else if (inTemplates) {
                if (trimmed.startsWith("- id:")) {
                    if (!current.id.isEmpty()) {
                        current.category = category;
                        if (current.os.isEmpty()) current.os = currentOs;
                        allItems.append(current);
                    }
                    current = PaletteItem();
                    current.id = trimmed.mid(5).trimmed();
                } else if (trimmed.startsWith("name:")) {
                    current.name = trimmed.mid(5).trimmed();
                } else if (trimmed.startsWith("cmd:")) {
                    current.cmd = trimmed.mid(4).trimmed();
                } else if (trimmed.startsWith("os:")) {
                    current.os = trimmed.mid(3).trimmed();
                } else if (trimmed.startsWith("description:")) {
                    current.description = trimmed.mid(12).trimmed();
                }
            }
        }
        
        if (!current.id.isEmpty()) {
            current.category = category;
            if (current.os.isEmpty()) current.os = currentOs;
            allItems.append(current);
        }
    }
}

void CommandPaletteDialog::updateResults(const QString& filter)
{
    resultList->clear();
    
    QString lowerFilter = filter.toLower();
    
    // 先显示历史记录
    if (filter.isEmpty()) {
        for (const QString& cmd : recentCommands) {
            auto item = new QListWidgetItem(QString("🕐 %1").arg(cmd));
            item->setData(Qt::UserRole, cmd);
            item->setData(Qt::UserRole + 1, true); // isRecent
            resultList->addItem(item);
        }
        
        if (!recentCommands.isEmpty()) {
            auto separator = new QListWidgetItem("── 全部命令 ──");
            separator->setFlags(Qt::NoItemFlags);
            separator->setForeground(QColor("#6a6a6a"));
            resultList->addItem(separator);
        }
    }
    
    // 显示匹配的模板
    int count = 0;
    for (const PaletteItem& item : allItems) {
        // OS 过滤
        if (!currentAgentOs.isEmpty() && !item.os.isEmpty() && 
            item.os != "any" && item.os != currentAgentOs) {
            continue;
        }
        
        // 搜索过滤
        if (!filter.isEmpty()) {
            bool match = item.name.toLower().contains(lowerFilter) ||
                        item.cmd.toLower().contains(lowerFilter) ||
                        item.description.toLower().contains(lowerFilter) ||
                        item.category.toLower().contains(lowerFilter);
            if (!match) continue;
        }
        
        QString displayText = QString("%1  [%2]").arg(item.name, item.os);
        auto listItem = new QListWidgetItem(displayText);
        listItem->setData(Qt::UserRole, item.cmd);
        listItem->setData(Qt::UserRole + 1, false);
        listItem->setData(Qt::UserRole + 2, item.description);
        listItem->setToolTip(item.cmd);
        resultList->addItem(listItem);
        
        count++;
        if (count >= 50) break; // 限制显示数量
    }
    
    if (resultList->count() > 0) {
        resultList->setCurrentRow(0);
    }
    
    statusLabel->setText(QString("找到 %1 条命令  ↑↓ 导航  Enter 执行  Esc 关闭").arg(count));
}

void CommandPaletteDialog::show()
{
    searchEdit->clear();
    updateResults("");
    
    // 居中显示
    if (parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2 - 50);
    }
    
    QDialog::show();
    searchEdit->setFocus();
}

void CommandPaletteDialog::setAgentContext(const QString& agentId, const QString& os)
{
    currentAgentId = agentId;
    currentAgentOs = os;
}

void CommandPaletteDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        executeSelected();
        return;
    }
    
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C) {
        auto item = resultList->currentItem();
        if (item) {
            QString cmd = item->data(Qt::UserRole).toString();
            QApplication::clipboard()->setText(cmd);
            statusLabel->setText("✓ 已复制到剪贴板");
        }
        return;
    }
    
    QDialog::keyPressEvent(event);
}

bool CommandPaletteDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        if (keyEvent->key() == Qt::Key_Up) {
            int row = resultList->currentRow();
            if (row > 0) {
                // 跳过分隔符
                int newRow = row - 1;
                while (newRow >= 0 && !(resultList->item(newRow)->flags() & Qt::ItemIsSelectable)) {
                    newRow--;
                }
                if (newRow >= 0) {
                    resultList->setCurrentRow(newRow);
                }
            }
            return true;
        }
        
        if (keyEvent->key() == Qt::Key_Down) {
            int row = resultList->currentRow();
            if (row < resultList->count() - 1) {
                // 跳过分隔符
                int newRow = row + 1;
                while (newRow < resultList->count() && !(resultList->item(newRow)->flags() & Qt::ItemIsSelectable)) {
                    newRow++;
                }
                if (newRow < resultList->count()) {
                    resultList->setCurrentRow(newRow);
                }
            }
            return true;
        }
    }
    
    return QDialog::eventFilter(obj, event);
}

void CommandPaletteDialog::executeSelected()
{
    auto item = resultList->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;
    
    QString cmd = item->data(Qt::UserRole).toString();
    if (cmd.isEmpty()) return;
    
    addToHistory(cmd);
    hide();
    
    // 执行命令
    if (currentAgentId.isEmpty()) {
        QMessageBox::warning(this, "执行命令", "请先选择一个 Agent");
        return;
    }
    
    Agent* agent = adaptixWidget->AgentsMap.value(currentAgentId, nullptr);
    if (agent && agent->Console) {
        agent->Console->SetInput(cmd);
        agent->Console->processInput();
    }
}

void CommandPaletteDialog::addToHistory(const QString& cmd)
{
    recentCommands.removeAll(cmd);
    recentCommands.prepend(cmd);
    
    while (recentCommands.size() > MAX_HISTORY) {
        recentCommands.removeLast();
    }
    
    saveHistory();
}

void CommandPaletteDialog::loadHistory()
{
    QSettings settings("Adaptix", "Client");
    recentCommands = settings.value("commandPalette/history").toStringList();
}

void CommandPaletteDialog::saveHistory()
{
    QSettings settings("Adaptix", "Client");
    settings.setValue("commandPalette/history", recentCommands);
}

void CommandPaletteDialog::onSearchChanged(const QString& text)
{
    updateResults(text);
}

void CommandPaletteDialog::onItemActivated(QListWidgetItem* item)
{
    Q_UNUSED(item);
    executeSelected();
}

void CommandPaletteDialog::onSelectionChanged()
{
    auto item = resultList->currentItem();
    if (item && (item->flags() & Qt::ItemIsSelectable)) {
        QString cmd = item->data(Qt::UserRole).toString();
        QString desc = item->data(Qt::UserRole + 2).toString();
        previewLabel->setText(cmd);
        if (!desc.isEmpty()) {
            previewLabel->setToolTip(desc);
        }
    }
}
