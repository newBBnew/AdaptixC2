#ifndef ADAPTIXCLIENT_COMMANDPALETTEDIALOG_H
#define ADAPTIXCLIENT_COMMANDPALETTEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QSettings>

class AdaptixWidget;
class CommandTemplatesWidget;

struct PaletteItem {
    QString id;
    QString name;
    QString cmd;
    QString category;
    QString os;
    QString description;
    bool isRecent = false;
};

class CommandPaletteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPaletteDialog(AdaptixWidget* parent);
    
    void show();
    void setAgentContext(const QString& agentId, const QString& os);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void createUI();
    void loadTemplates();
    void updateResults(const QString& filter);
    void executeSelected();
    void addToHistory(const QString& cmd);
    void loadHistory();
    void saveHistory();
    
    AdaptixWidget* adaptixWidget = nullptr;
    
    QLineEdit* searchEdit = nullptr;
    QListWidget* resultList = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statusLabel = nullptr;
    
    QList<PaletteItem> allItems;
    QStringList recentCommands;
    
    QString currentAgentId;
    QString currentAgentOs;
    
    static const int MAX_HISTORY = 20;

private Q_SLOTS:
    void onSearchChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onSelectionChanged();
};

#endif // ADAPTIXCLIENT_COMMANDPALETTEDIALOG_H
