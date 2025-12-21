#include <UI/Widgets/FileDeliveryWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <UI/Dialogs/DialogUploader.h>
#include <Utils/NonBlockingDialogs.h>

#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>

FileDeliveryWidget::FileDeliveryWidget(AdaptixWidget* w) : DockTab("FileDelivery", w->GetProfile()->GetProject(), ":/icons/upload")
{
    this->adaptixWidget = w;
    this->createUI();

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &FileDeliveryWidget::handleMenu);
    this->dockWidget->setWidget(this);

    refreshList();
}

FileDeliveryWidget::~FileDeliveryWidget() = default;

void FileDeliveryWidget::createUI()
{
    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(7);
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    tableWidget->setAutoFillBackground(false);
    tableWidget->setShowGrid(false);
    tableWidget->setSortingEnabled(true);
    tableWidget->setWordWrap(true);
    tableWidget->setCornerButtonEnabled(true);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget->setFocusPolicy(Qt::NoFocus);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setCascadingSectionResizes(true);
    tableWidget->horizontalHeader()->setHighlightSections(false);
    tableWidget->verticalHeader()->setVisible(false);

    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("File ID"));
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("File"));
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Size"));
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("SHA256"));
    tableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("URL"));
    tableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("Downloads"));
    tableWidget->setHorizontalHeaderItem(6, new QTableWidgetItem("Created"));
    tableWidget->hideColumn(0);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(tableWidget, 0, 0, 1, 4);
}

QString FileDeliveryWidget::currentFileId() const
{
    auto items = tableWidget->selectedItems();
    if (items.isEmpty())
        return QString();
    int row = items.first()->row();
    auto* it = tableWidget->item(row, 0);
    return it ? it->text() : QString();
}

QString FileDeliveryWidget::currentFileName() const
{
    auto items = tableWidget->selectedItems();
    if (items.isEmpty())
        return QString();
    int row = items.first()->row();
    auto* it = tableWidget->item(row, 1);
    return it ? it->text() : QString();
}

void FileDeliveryWidget::refreshList()
{
    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/filedelivery/files";
    QJsonObject jsonObject = HttpReqGet(sUrl, adaptixWidget->GetProfile()->GetAccessToken(), 15000);

    tableWidget->setSortingEnabled(false);
    tableWidget->setRowCount(0);

    if (!jsonObject.contains("ok") || !jsonObject["ok"].toBool() || !jsonObject.contains("data") || !jsonObject["data"].isArray()) {
        tableWidget->setSortingEnabled(true);
        return;
    }

    QJsonArray arr = jsonObject["data"].toArray();
    for (int i = 0; i < arr.size(); i++) {
        if (!arr[i].isObject())
            continue;
        QJsonObject o = arr[i].toObject();

        QString id = o.value("id").toString();
        QString name = o.value("file_name").toString();
        QString sha = o.value("sha256").toString();
        qint64 size = static_cast<qint64>(o.value("size").toDouble());
        QString created = o.value("created_at").toString();
        QString url = o.value("url").toString();
        int downloads = o.value("downloads").toInt();

        int row = tableWidget->rowCount();
        tableWidget->insertRow(row);

        auto* itemId = new QTableWidgetItem(id);
        auto* itemName = new QTableWidgetItem(name);
        auto* itemSize = new QTableWidgetItem(BytesToFormat(size));
        auto* itemSha = new QTableWidgetItem(sha);
        auto* itemUrl = new QTableWidgetItem(url);
        auto* itemDownloads = new QTableWidgetItem(QString::number(downloads));
        auto* itemCreated = new QTableWidgetItem(created);

        itemId->setFlags(itemId->flags() ^ Qt::ItemIsEditable);
        itemName->setFlags(itemName->flags() ^ Qt::ItemIsEditable);
        itemSize->setFlags(itemSize->flags() ^ Qt::ItemIsEditable);
        itemSha->setFlags(itemSha->flags() ^ Qt::ItemIsEditable);
        itemUrl->setFlags(itemUrl->flags() ^ Qt::ItemIsEditable);
        itemDownloads->setFlags(itemDownloads->flags() ^ Qt::ItemIsEditable);
        itemCreated->setFlags(itemCreated->flags() ^ Qt::ItemIsEditable);

        itemName->setToolTip(name);
        itemSha->setToolTip(sha);
        itemUrl->setToolTip(url);

        itemName->setTextAlignment(Qt::AlignCenter);
        itemSize->setTextAlignment(Qt::AlignCenter);
        itemDownloads->setTextAlignment(Qt::AlignCenter);
        itemCreated->setTextAlignment(Qt::AlignCenter);

        tableWidget->setItem(row, 0, itemId);
        tableWidget->setItem(row, 1, itemName);
        tableWidget->setItem(row, 2, itemSize);
        tableWidget->setItem(row, 3, itemSha);
        tableWidget->setItem(row, 4, itemUrl);
        tableWidget->setItem(row, 5, itemDownloads);
        tableWidget->setItem(row, 6, itemCreated);

        tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        tableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    }

    tableWidget->setSortingEnabled(true);
}

void FileDeliveryWidget::uploadFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select file");
    if (filePath.isEmpty())
        return;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        MessageError("Failed to read file");
        return;
    }
    QByteArray data = f.readAll();
    f.close();

    QUrl uploadUrl(adaptixWidget->GetProfile()->GetURL() + "/filedelivery/files");
    QString bearerToken = "Bearer " + adaptixWidget->GetProfile()->GetAccessToken();

    auto* uploaderDialog = new DialogUploader(uploadUrl, "Authorization", bearerToken, QFileInfo(filePath).fileName(), data);
    uploaderDialog->setAttribute(Qt::WA_DeleteOnClose);
    uploaderDialog->exec();

    refreshList();
}

void FileDeliveryWidget::createLink()
{
    QString fileId = currentFileId();
    if (fileId.isEmpty())
        return;

    QJsonObject dataJson;
    dataJson["file_id"] = fileId;
    dataJson["expire_hours"] = 24;
    dataJson["max_uses"] = 0;
    dataJson["allowed_ip"] = "";

    QByteArray jsonData = QJsonDocument(dataJson).toJson();
    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/filedelivery/links";
    QJsonObject jsonObject = HttpReq(sUrl, jsonData, adaptixWidget->GetProfile()->GetAccessToken(), 15000);

    if (!jsonObject.contains("ok") || !jsonObject["ok"].toBool() || !jsonObject.contains("data") || !jsonObject["data"].isObject())
        return;

    QJsonObject dataObj = jsonObject["data"].toObject();
    lastCreatedUrl = dataObj.value("url").toString();

    if (!lastCreatedUrl.isEmpty()) {
        QGuiApplication::clipboard()->setText(lastCreatedUrl);
        NonBlockingDialogs::information(this, "FileDelivery", "Link copied");
    }

    refreshList();
}

void FileDeliveryWidget::copyLink()
{
    if (lastCreatedUrl.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(lastCreatedUrl);
    NonBlockingDialogs::information(this, "FileDelivery", "Link copied");
}

void FileDeliveryWidget::deleteFile()
{
    QString fileId = currentFileId();
    if (fileId.isEmpty())
        return;

    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/filedelivery/files/" + fileId;
    QJsonObject jsonObject = HttpReqDelete(sUrl, adaptixWidget->GetProfile()->GetAccessToken(), 15000);

    if (jsonObject.contains("ok") && jsonObject["ok"].toBool())
        refreshList();
}

void FileDeliveryWidget::handleMenu(const QPoint &pos)
{
    QMenu contextMenu;

    QAction* actRefresh = contextMenu.addAction("Refresh");
    QAction* actUpload = contextMenu.addAction("Upload");
    QAction* actCreateLink = contextMenu.addAction("Create Link (Copy)");
    QAction* actCopyLink = contextMenu.addAction("Copy Last Link");
    QAction* actDelete = contextMenu.addAction("Delete");

    QAction* sel = contextMenu.exec(tableWidget->viewport()->mapToGlobal(pos));
    if (!sel)
        return;

    if (sel == actRefresh) refreshList();
    else if (sel == actUpload) uploadFile();
    else if (sel == actCreateLink) createLink();
    else if (sel == actCopyLink) copyLink();
    else if (sel == actDelete) deleteFile();
}
