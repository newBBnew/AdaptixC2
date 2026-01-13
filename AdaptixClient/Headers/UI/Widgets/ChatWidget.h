#ifndef ADAPTIXCLIENT_CHATWIDGET_H
#define ADAPTIXCLIENT_CHATWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QListWidget>

class AdaptixWidget;

class ChatWidget : public DockTab
{
    AdaptixWidget*   adaptixWidget  = nullptr;
    
    // Splitter for History | Chat
    QSplitter*       mainSplitter   = nullptr;
    
    // Left Side: History
    QWidget*         historyWidget  = nullptr;
    QVBoxLayout*     historyLayout  = nullptr;
    QLabel*          historyLabel   = nullptr;
    QListWidget*     historyList    = nullptr;
    
    // Right Side: Active Chat
    QWidget*         chatContainer  = nullptr;
    QLabel*          usernameLabel  = nullptr;
    QPlainTextEdit*  chatInput      = nullptr;
    TextEditConsole* chatTextEdit   = nullptr;
    QGridLayout*     chatGridLayout = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    ClickableLabel* prevButton     = nullptr;
    ClickableLabel* nextButton     = nullptr;
    QLabel*         searchLabel    = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;
    QSpacerItem*    spacer         = nullptr;
    QShortcut*      shortcutSearch = nullptr;

    int  currentIndex = -1;
    QVector<QTextEdit::ExtraSelection> allSelections;

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;
    void loadSessionList();

public:
    explicit ChatWidget(AdaptixWidget* w);
    ~ChatWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddChatMessage(qint64 time, const QString &username, const QString &message);
    void Clear() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public Q_SLOTS:
    void handleChat();
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
    void onExtendContextMenu(QMenu* menu);
    void handleArchiveChat();
    void onSessionClicked(QListWidgetItem* item);
    void onHistoryContextMenu(const QPoint& pos);
};

#endif //ADAPTIXCLIENT_CHATWIDGET_H