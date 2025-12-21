#include <UI/Widgets/HostedFilesWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/AuthProfile.h>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QVBoxLayout>

HostedFilesWidget::HostedFilesWidget(AdaptixWidget* w) : DockTab("托管文件", w->GetProfile()->GetProject())
{
    adaptixWidget = w;
    this->createUI();
    this->setupConnections();
    
    // 初始加载文件列表
    refreshFilesList();
    
    this->dockWidget->setWidget(this);
}

HostedFilesWidget::~HostedFilesWidget() = default;

void HostedFilesWidget::createUI()
{
    // 创建主布局
    mainGridLayout = new QGridLayout(this);
    
    // 创建工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    buttonUpload = new QPushButton(QIcon(":/icons/upload"), "上传文件", this);
    buttonUpload->setIconSize(QSize(24, 24));
    buttonUpload->setToolTip("上传新文件");
    
    buttonRefresh = new QPushButton(QIcon(":/icons/reload"), "刷新", this);
    buttonRefresh->setIconSize(QSize(24, 24));
    buttonRefresh->setToolTip("刷新文件列表");
    
    buttonDelete = new QPushButton(QIcon(":/icons/delete"), "删除", this);
    buttonDelete->setIconSize(QSize(24, 24));
    buttonDelete->setToolTip("删除选中的文件");
    buttonDelete->setEnabled(false);
    
    buttonCleanup = new QPushButton(QIcon(":/icons/clean"), "清理过期", this);
    buttonCleanup->setIconSize(QSize(24, 24));
    buttonCleanup->setToolTip("清理过期文件");
    
    statusLabel = new QLabel("就绪", this);
    statusLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    
    toolbarLayout->addWidget(buttonUpload);
    toolbarLayout->addWidget(buttonRefresh);
    toolbarLayout->addWidget(buttonDelete);
    toolbarLayout->addWidget(buttonCleanup);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
    
    // 创建表格
    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(8);
    
    QStringList headers;
    headers << "文件名" << "大小" << "类型" << "上传者" << "上传时间" << "过期时间" << "下载链接" << "状态";
    tableWidget->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列宽
    tableWidget->setColumnWidth(0, 200); // 文件名
    tableWidget->setColumnWidth(1, 100); // 大小
    tableWidget->setColumnWidth(2, 150); // 类型
    tableWidget->setColumnWidth(3, 100); // 上传者
    tableWidget->setColumnWidth(4, 150); // 上传时间
    tableWidget->setColumnWidth(5, 150); // 过期时间
    tableWidget->setColumnWidth(6, 300); // 下载链接
    
    // 添加到主布局
    mainGridLayout->addLayout(toolbarLayout, 0, 0);
    mainGridLayout->addWidget(tableWidget, 1, 0);
}

void HostedFilesWidget::setupConnections()
{
    connect(buttonUpload, &QPushButton::clicked, this, &HostedFilesWidget::actionUpload);
    connect(buttonRefresh, &QPushButton::clicked, this, &HostedFilesWidget::actionRefresh);
    connect(buttonDelete, &QPushButton::clicked, this, &HostedFilesWidget::actionDelete);
    connect(buttonCleanup, &QPushButton::clicked, this, &HostedFilesWidget::actionCleanup);
    
    connect(tableWidget, &QTableWidget::customContextMenuRequested, 
            this, &HostedFilesWidget::handleTableMenu);
    connect(tableWidget, &QTableWidget::itemSelectionChanged, [this]() {
        buttonDelete->setEnabled(tableWidget->selectedItems().size() > 0);
    });
}

void HostedFilesWidget::refreshFilesList()
{
    statusLabel->setText("正在加载...");
    
    QJsonObject request;
    request["action"] = "hosted_list";
    
    adaptixWidget->GetProfile()->SendRequest("hosted/list", request, 
        [this](const QString& response) {
            handleHostedFilesResponse(response);
        });
}

void HostedFilesWidget::formatFileSize(qint64 size, QString& formattedSize) const
{
    if (size < 1024) {
        formattedSize = QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        formattedSize = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else if (size < 1024 * 1024 * 1024) {
        formattedSize = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        formattedSize = QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
}

void HostedFilesWidget::Clear() const
{
    tableWidget->setRowCount(0);
}

void HostedFilesWidget::AddHostedFileItem(const HostedFileData &newHostedFile)
{
    int row = tableWidget->rowCount();
    tableWidget->insertRow(row);
    
    // 文件名
    QTableWidgetItem* nameItem = new QTableWidgetItem(newHostedFile.fileName);
    nameItem->setToolTip(newHostedFile.fileName);
    tableWidget->setItem(row, 0, nameItem);
    
    // 大小
    QString sizeStr;
    formatFileSize(newHostedFile.fileSize, sizeStr);
    tableWidget->setItem(row, 1, new QTableWidgetItem(sizeStr));
    
    // 类型
    tableWidget->setItem(row, 2, new QTableWidgetItem(newHostedFile.contentType));
    
    // 上传者
    tableWidget->setItem(row, 3, new QTableWidgetItem(newHostedFile.uploadUser));
    
    // 上传时间
    tableWidget->setItem(row, 4, new QTableWidgetItem(newHostedFile.uploadTime.toString("yyyy-MM-dd hh:mm:ss")));
    
    // 过期时间
    tableWidget->setItem(row, 5, new QTableWidgetItem(newHostedFile.expireTime.toString("yyyy-MM-dd hh:mm:ss")));
    
    // 下载链接
    QTableWidgetItem* urlItem = new QTableWidgetItem(newHostedFile.downloadURL);
    urlItem->setToolTip(newHostedFile.downloadURL);
    tableWidget->setItem(row, 6, urlItem);
    
    // 状态
    QString statusStr;
    switch (newHostedFile.state) {
        case 1: statusStr = "活跃"; break;
        case 2: statusStr = "已删除"; break;
        case 3: statusStr = "已过期"; break;
        default: statusStr = "未知"; break;
    }
    tableWidget->setItem(row, 7, new QTableWidgetItem(statusStr));
}

void HostedFilesWidget::UpdateHostedFileItem(const QString &fileId, const HostedFileData &updatedFile)
{
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == fileId) {
            // 更新现有行的数据
            tableWidget->item(row, 0)->setText(updatedFile.fileName);
            
            QString sizeStr;
            formatFileSize(updatedFile.fileSize, sizeStr);
            tableWidget->item(row, 1)->setText(sizeStr);
            tableWidget->item(row, 2)->setText(updatedFile.contentType);
            tableWidget->item(row, 3)->setText(updatedFile.uploadUser);
            tableWidget->item(row, 4)->setText(updatedFile.uploadTime.toString("yyyy-MM-dd hh:mm:ss"));
            tableWidget->item(row, 5)->setText(updatedFile.expireTime.toString("yyyy-MM-dd hh:mm:ss"));
            tableWidget->item(row, 6)->setText(updatedFile.downloadURL);
            
            QString statusStr;
            switch (updatedFile.state) {
                case 1: statusStr = "活跃"; break;
                case 2: statusStr = "已删除"; break;
                case 3: statusStr = "已过期"; break;
                default: statusStr = "未知"; break;
            }
            tableWidget->item(row, 7)->setText(statusStr);
            break;
        }
    }
}

void HostedFilesWidget::RemoveHostedFileItem(const QString &fileId) const
{
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == fileId) {
            tableWidget->removeRow(row);
            break;
        }
    }
}

void HostedFilesWidget::handleTableMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    
    QAction* copyURLAction = contextMenu.addAction(QIcon(":/icons/copy"), "复制下载链接");
    QAction* downloadAction = contextMenu.addAction(QIcon(":/icons/download"), "下载文件");
    contextMenu.addSeparator();
    QAction* deleteAction = contextMenu.addAction(QIcon(":/icons/delete"), "删除文件");
    
    QTableWidgetItem* item = tableWidget->itemAt(pos);
    if (!item) return;
    
    QAction* selectedAction = contextMenu.exec(tableWidget->mapToGlobal(pos));
    
    if (selectedAction == copyURLAction) {
        actionCopyURL();
    } else if (selectedAction == downloadAction) {
        actionDownloadFile();
    } else if (selectedAction == deleteAction) {
        actionDelete();
    }
}

void HostedFilesWidget::actionUpload()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择要上传的文件");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + fileName);
        return;
    }
    
    QByteArray fileData = file.readAll();
    file.close();
    
    QFileInfo fileInfo(fileName);
    
    // 构建上传请求
    QJsonObject request;
    request["file_name"] = fileInfo.fileName();
    request["file_size"] = static_cast<qint64>(fileData.size());
    request["content_type"] = "application/octet-stream";
    request["expire_hours"] = 24; // 默认24小时过期
    request["content"] = QString(fileData.toBase64());
    
    statusLabel->setText("正在上传...");
    
    adaptixWidget->GetProfile()->SendRequest("hosted/upload", request,
        [this](const QString& response) {
            handleUploadResponse(response);
        });
}

void HostedFilesWidget::actionRefresh()
{
    refreshFilesList();
}

void HostedFilesWidget::actionDelete()
{
    int currentRow = tableWidget->currentRow();
    if (currentRow < 0) return;
    
    QTableWidgetItem* item = tableWidget->item(currentRow, 0);
    if (!item) return;
    
    QString fileId = item->data(Qt::UserRole).toString();
    if (fileId.isEmpty()) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除这个文件吗？",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        QJsonObject request;
        request["file_id"] = fileId;
        
        adaptixWidget->GetProfile()->SendRequest("hosted/delete", request,
            [this](const QString& response) {
                handleDeleteResponse(response);
            });
    }
}

void HostedFilesWidget::actionCleanup()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清理", "确定要清理所有过期文件吗？",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        QJsonObject request;
        
        adaptixWidget->GetProfile()->SendRequest("hosted/cleanup", request,
            [this](const QString& response) {
                // 清理完成后刷新列表
                refreshFilesList();
            });
    }
}

void HostedFilesWidget::actionCopyURL()
{
    int currentRow = tableWidget->currentRow();
    if (currentRow < 0) return;
    
    QTableWidgetItem* urlItem = tableWidget->item(currentRow, 6);
    if (!urlItem) return;
    
    QString url = urlItem->text();
    QApplication::clipboard()->setText(url);
    
    QMessageBox::information(this, "复制成功", "下载链接已复制到剪贴板");
}

void HostedFilesWidget::actionDownloadFile()
{
    int currentRow = tableWidget->currentRow();
    if (currentRow < 0) return;
    
    QTableWidgetItem* urlItem = tableWidget->item(currentRow, 6);
    if (!urlItem) return;
    
    QString url = urlItem->text();
    QDesktopServices::openUrl(QUrl(url));
}

void HostedFilesWidget::handleHostedFilesResponse(const QString& response)
{
    statusLabel->setText("就绪");
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "错误", "解析响应失败: " + error.errorString());
        return;
    }
    
    Clear();
    
    if (doc.isArray()) {
        QJsonArray array = doc.array();
        for (const QJsonValue& value : array) {
            QJsonObject obj = value.toObject();
            
            HostedFileData data;
            data.fileId = obj["file_id"].toString();
            data.fileName = obj["file_name"].toString();
            data.fileSize = obj["file_size"].toVariant().toLongLong();
            data.contentType = obj["content_type"].toString();
            data.uploadUser = obj["upload_user"].toString();
            data.uploadTime = QDateTime::fromSecsSinceEpoch(obj["upload_time"].toVariant().toLongLong());
            data.expireTime = QDateTime::fromSecsSinceEpoch(obj["expire_time"].toVariant().toLongLong());
            data.downloadURL = obj["download_url"].toString();
            data.state = obj["state"].toInt();
            
            AddHostedFileItem(data);
        }
    }
}

void HostedFilesWidget::handleUploadResponse(const QString& response)
{
    statusLabel->setText("就绪");
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "错误", "解析响应失败: " + error.errorString());
        return;
    }
    
    QJsonObject obj = doc.object();
    if (obj["ok"].toBool()) {
        QMessageBox::information(this, "上传成功", 
            "文件上传成功！\n下载链接: " + obj["download_url"].toString());
        refreshFilesList();
    } else {
        QMessageBox::warning(this, "上传失败", obj["message"].toString());
    }
}

void HostedFilesWidget::handleDeleteResponse(const QString& response)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "错误", "解析响应失败: " + error.errorString());
        return;
    }
    
    QJsonObject obj = doc.object();
    if (obj["ok"].toBool()) {
        QMessageBox::information(this, "删除成功", "文件已删除");
        refreshFilesList();
    } else {
        QMessageBox::warning(this, "删除失败", obj["message"].toString());
    }
}
