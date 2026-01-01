#include <UI/Widgets/FileDeliveryWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>

REGISTER_DOCK_WIDGET(FileDeliveryWidget, "File Delivery", true)

FileDeliveryWidget::FileDeliveryWidget(AdaptixWidget* w) 
    : DockTab("File Delivery", w->GetProfile()->GetProject(), ":/icons/storage"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView, &QTableView::customContextMenuRequested, this, &FileDeliveryWidget::handleContextMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FileDeliveryWidget::onSelectionChanged);
    connect(hideButton, &ClickableLabel::clicked, this, &FileDeliveryWidget::toggleSearchPanel);
    connect(inputFilter, &QLineEdit::textChanged, this, &FileDeliveryWidget::onFilterUpdate);
    connect(inputFilter, &QLineEdit::returnPressed, this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &FileDeliveryWidget::toggleSearchPanel);

    this->dockWidget->setWidget(this);
}

FileDeliveryWidget::~FileDeliveryWidget() = default;

void FileDeliveryWidget::SetUpdatesEnabled(bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void FileDeliveryWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("Filter files...");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setStyleSheet("QLabel { color: #888; font-weight: bold; } QLabel:hover { color: #e34234; }");

    auto searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 5, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    tableModel = new FileDeliveryTableModel(this);
    proxyModel = new FileDeliveryFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->hideColumn(DC_FileId_FD);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView, 1, 0, 1, 1);
}

void FileDeliveryWidget::AddFileItem(const FileDeliveryData &item)
{
    tableModel->add(item);
}

void FileDeliveryWidget::UpdateFileItem(const QString &fileId, const QString &url, int downloads)
{
    tableModel->update(fileId, url, downloads);
}

void FileDeliveryWidget::RemoveFileItems(const QStringList &ids)
{
    tableModel->remove(ids);
}

void FileDeliveryWidget::Clear()
{
    tableModel->clear();
}

void FileDeliveryWidget::UpdateColumnsSize()
{
    tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(DC_URL_FD, QHeaderView::Stretch);
}

QString FileDeliveryWidget::getSelectedFileId() const
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return {};
    QModelIndex sourceIndex = proxyModel->mapToSource(selected.first());
    return tableModel->getFileIdAt(sourceIndex.row());
}

void FileDeliveryWidget::toggleSearchPanel()
{
    bool visible = !searchWidget->isVisible();
    searchWidget->setVisible(visible);
    proxyModel->setSearchVisible(visible);
    if (visible) inputFilter->setFocus();
}

void FileDeliveryWidget::onFilterUpdate()
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
}

void FileDeliveryWidget::onSelectionChanged()
{
    tableView->setFocus();
}

void FileDeliveryWidget::handleContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QString selectedId = getSelectedFileId();

    menu.addAction("Upload File", this, &FileDeliveryWidget::actionUpload);
    
    if (!selectedId.isEmpty()) {
        menu.addSeparator();
        menu.addAction("Create Download Link", this, &FileDeliveryWidget::actionCreateLink);
        
        auto* data = tableModel->getById(selectedId);
        if (data && !data->URL.isEmpty()) {
            menu.addAction("Copy Download URL", this, [data]() {
                QApplication::clipboard()->setText(data->URL);
            });
        }

        menu.addAction("Delete Hosted File", this, &FileDeliveryWidget::actionDelete);
    }

    menu.exec(tableView->viewport()->mapToGlobal(pos));
}

void FileDeliveryWidget::actionUpload()
{
    QString localPath = QFileDialog::getOpenFileName(this, "Select file to host", "", "All Files (*)");
    if (localPath.isEmpty()) return;

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        MessageError("Failed to open local file");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QString fileName = QFileInfo(localPath).fileName();
    HttpReqFileDeliveryUploadAsync(fileName, data, *adaptixWidget->GetProfile(), [](bool success, const QString& message, const QJsonObject&){
        if (!success) MessageError(message);
    });
}

void FileDeliveryWidget::actionDelete()
{
    QString fileId = getSelectedFileId();
    if (fileId.isEmpty()) return;

    if (QMessageBox::question(this, "Delete File", "Are you sure you want to delete this hosted file?") != QMessageBox::Yes)
        return;

    HttpReqFileDeliveryDeleteAsync(fileId, *adaptixWidget->GetProfile(), [](bool success, const QString& message, const QJsonObject&){
        if (!success) MessageError(message);
    });
}

void FileDeliveryWidget::actionCreateLink()
{
    QString fileId = getSelectedFileId();
    if (fileId.isEmpty()) return;

    bool ok;
    int hours = QInputDialog::getInt(this, "Link Expiration", "Expiration (hours):", 24, 1, 8760, 1, &ok);
    if (!ok) return;

    HttpReqFileDeliveryLinkCreateAsync(fileId, hours, 0, "", *adaptixWidget->GetProfile(), [this](bool success, const QString& message, const QJsonObject& data){
        if (success) {
            QString token = data["token"].toString();
            QString url = adaptixWidget->GetProfile()->GetURL() + "/download/" + token;
            QInputDialog::getText(this, "Link Created", "Download URL:", QLineEdit::Normal, url, &success);
        } else {
            MessageError(message);
        }
    });
}
