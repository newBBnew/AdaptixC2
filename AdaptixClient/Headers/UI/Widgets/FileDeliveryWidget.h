#ifndef ADAPTIXCLIENT_FILEDELIVERYWIDGET_H
#define ADAPTIXCLIENT_FILEDELIVERYWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

class FileDeliveryWidget : public DockTab
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QGridLayout*   mainGridLayout = nullptr;

    void createUI();
    QString currentFileId() const;
    QString currentFileName() const;

public:
    FileDeliveryWidget(AdaptixWidget* w);
    ~FileDeliveryWidget() override;

public Q_SLOTS:
    void refreshList();
    void uploadFile();
    void createLink();
    void copyLink();
    void deleteFile();
    void handleMenu(const QPoint &pos);

private:
    QString lastCreatedUrl;
};

#endif
