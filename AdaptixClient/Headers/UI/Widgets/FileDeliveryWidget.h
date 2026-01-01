#ifndef ADAPTIXCLIENT_FILEDELIVERYWIDGET_H
#define ADAPTIXCLIENT_FILEDELIVERYWIDGET_H

#define DC_FileId_FD    0
#define DC_Name_FD      1
#define DC_Size_FD      2
#define DC_Sha256_FD    3
#define DC_Type_FD      4
#define DC_URL_FD       5
#define DC_Downloads_FD 6
#define DC_Date_FD      7
#define DC_ColumnCount_FD 8

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <QSortFilterProxyModel>

class AdaptixWidget;
class ClickableLabel;

class FileDeliveryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QString filter;
    bool searchVisible = false;

public:
    explicit FileDeliveryFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setSortRole(Qt::UserRole);
    }

    void setSearchVisible(bool visible) {
        if (searchVisible == visible) return;
        searchVisible = visible;
        invalidateFilter();
    }

    void setTextFilter(const QString &text) {
        if (filter == text) return;
        filter = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        auto model = sourceModel();
        if (!model) return true;
        if (!searchVisible) return true;

        if (!filter.isEmpty()) {
            QString rowData;
            for (int i = 0; i < DC_ColumnCount_FD; ++i) {
                rowData += model->index(row, i, parent).data().toString() + " ";
            }
            if (!rowData.contains(filter, Qt::CaseInsensitive))
                return false;
        }
        return true;
    }
};

class FileDeliveryTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QVector<FileDeliveryData> files;
    QHash<QString, int> idToRow;

public:
    explicit FileDeliveryTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return files.size(); }
    int columnCount(const QModelIndex&) const override { return DC_ColumnCount_FD; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= files.size())
            return {};

        const FileDeliveryData& f = files.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case DC_FileId_FD: return f.FileId;
                case DC_Name_FD:   return f.Name;
                case DC_Size_FD:   return BytesToFormat(f.Size);
                case DC_Sha256_FD: return f.Sha256;
                case DC_Type_FD:   return f.Type;
                case DC_URL_FD:    return f.URL;
                case DC_Downloads_FD: return f.Downloads;
                case DC_Date_FD:   return f.Date;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case DC_Size_FD: return static_cast<qint64>(f.Size);
                case DC_Downloads_FD: return f.Downloads;
                case DC_Date_FD: return f.DateTimestamp;
                default:         return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole)
            return Qt::AlignCenter;

        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "File ID", "Name", "Size", "SHA256", "Type", "Download URL", "Count", "Date"
        };
        return headers.value(section);
    }

    void add(const FileDeliveryData& item) {
        if (idToRow.contains(item.FileId)) return;
        const int row = files.size();
        beginInsertRows(QModelIndex(), row, row);
        files.append(item);
        idToRow[item.FileId] = row;
        endInsertRows();
    }

    void update(const QString& fileId, const QString& url, int downloads = -1) {
        auto it = idToRow.find(fileId);
        if (it == idToRow.end()) return;
        int row = it.value();
        if (!url.isEmpty()) files[row].URL = url;
        if (downloads >= 0) files[row].Downloads = downloads;
        Q_EMIT dataChanged(index(row, 0), index(row, DC_ColumnCount_FD - 1));
    }

    void remove(const QStringList& ids) {
        for (const QString& id : ids) {
            auto it = idToRow.find(id);
            if (it != idToRow.end()) {
                int row = it.value();
                beginRemoveRows(QModelIndex(), row, row);
                files.removeAt(row);
                idToRow.remove(id);
                endRemoveRows();
                // Rebuild index after removal to keep it valid
                for (int i = row; i < files.size(); ++i)
                    idToRow[files[i].FileId] = i;
            }
        }
    }

    void clear() {
        beginResetModel();
        files.clear();
        idToRow.clear();
        endResetModel();
    }

    QString getFileIdAt(int row) const {
        if (row < 0 || row >= files.size()) return {};
        return files.at(row).FileId;
    }
    
    const FileDeliveryData* getById(const QString& id) const {
        auto it = idToRow.find(id);
        if (it == idToRow.end()) return nullptr;
        return &files.at(it.value());
    }
};

class FileDeliveryWidget : public DockTab
{
    Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;
    QGridLayout* mainGridLayout = nullptr;
    QTableView* tableView = nullptr;
    QShortcut* shortcutSearch = nullptr;

    FileDeliveryTableModel* tableModel = nullptr;
    FileDeliveryFilterProxyModel* proxyModel = nullptr;

    QWidget* searchWidget = nullptr;
    QLineEdit* inputFilter = nullptr;
    QCheckBox* autoSearchCheck = nullptr;
    ClickableLabel* hideButton = nullptr;

    void createUI();

public:
    explicit FileDeliveryWidget(AdaptixWidget* w);
    ~FileDeliveryWidget() override;

    void SetUpdatesEnabled(bool enabled);
    void AddFileItem(const FileDeliveryData &item);
    void UpdateFileItem(const QString &fileId, const QString &url, int downloads = -1);
    void RemoveFileItems(const QStringList &ids);
    void Clear();
    void UpdateColumnsSize();

    QString getSelectedFileId() const;

public Q_SLOTS:
    void toggleSearchPanel();
    void onFilterUpdate();
    void handleContextMenu(const QPoint &pos);
    void actionUpload();
    void actionDelete();
    void actionCreateLink();
    void onSelectionChanged();
};

#endif
