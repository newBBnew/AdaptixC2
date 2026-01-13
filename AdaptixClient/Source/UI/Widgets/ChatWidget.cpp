#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Settings.h>
#include <Utils/Convert.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Workers/MCP/MCPBridgeWorker.h>
#include <QKeyEvent>
#include <QMenu>
#include <QDateTime>

REGISTER_DOCK_WIDGET(ChatWidget, "Chat", true)

ChatWidget::ChatWidget(AdaptixWidget* w) : DockTab("Chat", w->GetProfile()->GetProject(), ":/icons/chat"), adaptixWidget(w)
{
    this->createUI();

    // connect(chatInput,      &QLineEdit::returnPressed,  this, &ChatWidget::handleChat); // Removed
    connect(searchLineEdit, &QLineEdit::returnPressed,  this, &ChatWidget::handleSearch);
    connect(nextButton,     &ClickableLabel::clicked,   this, &ChatWidget::handleSearch);
    connect(prevButton,     &ClickableLabel::clicked,   this, &ChatWidget::handleSearchBackward);
    connect(hideButton,     &ClickableLabel::clicked,   this, &ChatWidget::toggleSearchPanel);
    connect(chatTextEdit,   &TextEditConsole::ctx_find, this, &ChatWidget::toggleSearchPanel);
    connect(chatTextEdit,   &TextEditConsole::extendContextMenu, this, &ChatWidget::onExtendContextMenu);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ChatWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, chatTextEdit, &QTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, chatTextEdit, &QTextEdit::selectAll);

    connect(historyList, &QListWidget::itemClicked, this, &ChatWidget::onSessionClicked);

    this->dockWidget->setWidget(this);
    
    // Initial load of session list
    loadSessionList();
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::SetUpdatesEnabled(const bool enabled)
{
    chatTextEdit->setUpdatesEnabled(enabled);
}

void ChatWidget::createUI()
{
    // --- History Panel ---
    historyWidget = new QWidget(this);
    historyLayout = new QVBoxLayout(historyWidget);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    
    historyLabel = new QLabel("History Sessions");
    historyLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    historyLabel->setAlignment(Qt::AlignCenter);
    
    historyList = new QListWidget(this);
    historyList->setAlternatingRowColors(true);
    historyList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(historyList, &QListWidget::customContextMenuRequested, this, &ChatWidget::onHistoryContextMenu);
    
    // Add "Current Session" item
    QListWidgetItem* currentItem = new QListWidgetItem("Current Session");
    currentItem->setData(Qt::UserRole, ""); // Empty ID for current
    historyList->addItem(currentItem);
    historyList->setCurrentItem(currentItem);

    historyLayout->addWidget(historyLabel);
    historyLayout->addWidget(historyList);

    // --- Active Chat Panel ---
    chatContainer = new QWidget(this);
    
    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    prevButton = new ClickableLabel("<");
    prevButton->setCursor( Qt::PointingHandCursor );

    nextButton = new ClickableLabel(">");
    nextButton->setCursor( Qt::PointingHandCursor );

    searchLabel    = new QLabel("0 of 0");
    searchLineEdit = new QLineEdit();
    searchLineEdit->setPlaceholderText("Find");
    searchLineEdit->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor( Qt::PointingHandCursor );

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 3, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(prevButton);
    searchLayout->addWidget(nextButton);
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(spacer);

    usernameLabel = new QLabel(this );
    usernameLabel->setProperty( "LabelStyle", "console" );
    usernameLabel->setText( adaptixWidget->GetProfile()->GetUsername() );

    chatInput = new QPlainTextEdit(this);
    chatInput->setProperty( "LineEditStyle", "console" ); // Keep style property, might work or fallback
    chatInput->setMaximumHeight(60);
    chatInput->installEventFilter(this);

    chatTextEdit = new TextEditConsole(this);
    chatTextEdit->setReadOnly(true);
    chatTextEdit->setProperty("TextEditStyle", "console" );

    chatGridLayout = new QGridLayout(chatContainer);
    chatGridLayout->setContentsMargins(0, 1, 0, 4);
    chatGridLayout->setVerticalSpacing(4);
    chatGridLayout->addWidget( searchWidget,  0, 0, 1, 1);
    chatGridLayout->addWidget( chatTextEdit,  1, 0, 1, 2);
    chatGridLayout->addWidget( usernameLabel, 2, 0, 1, 1);
    chatGridLayout->addWidget( chatInput,     2, 1, 1, 1);

    // --- Main Splitter ---
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(historyWidget);
    mainSplitter->addWidget(chatContainer);
    mainSplitter->setStretchFactor(1, 1); // Give chat more space
    mainSplitter->setCollapsible(0, true);
    
    // Main layout for this widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(mainSplitter);
}

void ChatWidget::handleChat()
{
    QString text = chatInput->toPlainText().trimmed();
    if (text.isEmpty()) return;
    
    chatInput->clear();
    
    HttpReqChatSendMessageAsync(text, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == chatInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {
                handleChat();
                return true;
            }
        }
    }
    return DockTab::eventFilter(obj, event);
}


void ChatWidget::AddChatMessage(const qint64 time, const QString &username, const QString &message)
{
    // Only add message if we are viewing the current session
    QListWidgetItem* current = historyList->currentItem();
    if (current && !current->data(Qt::UserRole).toString().isEmpty()) {
        // We are viewing history, ignore live messages or maybe show a notification dot
        return; 
    }

    // LogInfo("[ChatWidget] Adding message from %s: %s", username.toUtf8().constData(), message.toUtf8().constData());
    bool isDarkIce = (GlobalClient->settings->data.MainTheme == "Dark_Ice");
    bool isGlass = (GlobalClient->settings->data.MainTheme == "Glass_Morph");
    bool isHackerTech = (GlobalClient->settings->data.MainTheme == "Hacker_Tech");
    chatTextEdit->appendColor(UnixTimestampGlobalToStringLocal(time), QColor(COLOR_Gray));
    chatTextEdit->appendPlain(" [");
    if (username == adaptixWidget->GetProfile()->GetUsername())
        chatTextEdit->appendColor(username, (isDarkIce || isGlass || isHackerTech) ? QColor(COLOR_IceBlue) : QColor(COLOR_NeonGreen));
    else
        chatTextEdit->appendColor(username, isDarkIce ? QColor("#00C2FF") : (isHackerTech ? QColor("#00F0FF") : QColor(COLOR_KellyGreen)));
    chatTextEdit->appendPlain("] :: ");
    chatTextEdit->appendColor(message, isDarkIce ? QColor(COLOR_IceBlue) : (isHackerTech ? QColor("#B6FFCC") : QColor(COLOR_ConsoleWhite)));
    chatTextEdit->appendPlain("\n");
}


void ChatWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(chatTextEdit->document());
    cursor.movePosition(QTextCursor::Start);

    bool isDarkIce = (GlobalClient->settings->data.MainTheme == "Dark_Ice");
    bool isGlass = (GlobalClient->settings->data.MainTheme == "Glass_Morph");
    bool isHackerTech = (GlobalClient->settings->data.MainTheme == "Hacker_Tech");
    QTextCharFormat baseFmt;
    if (isDarkIce || isGlass || isHackerTech) {
        baseFmt.setBackground(QColor(0, 240, 255, 100));
        baseFmt.setForeground(Qt::white);
    } else {
        baseFmt.setBackground(Qt::blue);
        baseFmt.setForeground(Qt::white);
    }

    while (true) {
        auto found = chatTextEdit->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    chatTextEdit->setExtraSelections(allSelections);
}

void ChatWidget::highlightCurrent() const
{
    if (allSelections.isEmpty()) {
        searchLabel->setText("0 of 0");
        return;
    }

    auto sels = allSelections;

    QTextCharFormat activeFmt;
    activeFmt.setBackground(Qt::white);
    activeFmt.setForeground(Qt::black);

    sels[currentIndex].format = activeFmt;

    chatTextEdit->setExtraSelections(sels);
    chatTextEdit->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}

void ChatWidget::Clear() const { chatTextEdit->clear(); }

void ChatWidget::toggleSearchPanel()
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        searchLineEdit->setText("");
        handleSearch();
    }
    else {
        this->searchWidget->setVisible(true);
        searchLineEdit->setFocus();
        searchLineEdit->selectAll();
    }
}

void ChatWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if ( pattern.isEmpty() && allSelections.size() ) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        chatTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    }
    else {
        currentIndex = (currentIndex + 1) % allSelections.size();
    }

    highlightCurrent();
}

void ChatWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        chatTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = allSelections.size() - 1;
    }
    else {
        currentIndex = (currentIndex - 1 + allSelections.size()) % allSelections.size();
    }

    highlightCurrent();
}

void ChatWidget::onExtendContextMenu(QMenu* menu)
{
    menu->addSeparator();
    QAction* archiveAction = menu->addAction("Archive Current Session (Start Fresh)");
    connect(archiveAction, &QAction::triggered, this, &ChatWidget::handleArchiveChat);
}

void ChatWidget::handleArchiveChat()
{
    HttpReqSessionArchiveAsync(*(adaptixWidget->GetProfile()), [this](bool success, const QString& message, const QJsonObject& data) {
        if (success) {
            QString sessionId = data["session_id"].toString();
            LogInfo("Session archived: %s", sessionId.toUtf8().constData());
            
            // Notify MCP to clear context
            if (adaptixWidget->McpBridge) {
                QJsonObject params;
                params["timestamp"] = QDateTime::currentSecsSinceEpoch();
                params["session_id"] = sessionId;
                adaptixWidget->McpBridge->sendMessage("tactical_archive", params);
            }

            // Clear current chat
            this->Clear();
            
            // Add system message
            this->AddChatMessage(QDateTime::currentSecsSinceEpoch(), "System", "Session has been archived.");
            
            // Reload session list
            loadSessionList();
        } else {
            MessageError("Failed to archive session: " + message);
        }
    });
}

void ChatWidget::loadSessionList()
{
    HttpReqSessionListAsync(*(adaptixWidget->GetProfile()), [this](bool success, const QString& message, const QJsonObject& data) {
        if (success) {
            historyList->clear();
            
            // Add "Current Session" item first
            QListWidgetItem* currentItem = new QListWidgetItem("Current Session");
            currentItem->setData(Qt::UserRole, ""); 
            historyList->addItem(currentItem);
            
            QJsonArray sessions = data["sessions"].toArray();
            for (const auto& sessVal : sessions) {
                QJsonObject sess = sessVal.toObject();
                QString id = sess["id"].toString();
                QString name = sess["name"].toString();
                qint64 start = sess["start"].toVariant().toLongLong();
                
                QString label = QString("%1 (%2)").arg(name).arg(UnixTimestampGlobalToStringLocal(start));
                
                QListWidgetItem* item = new QListWidgetItem(label);
                item->setData(Qt::UserRole, id);
                historyList->addItem(item);
            }
            
            // Re-select current session if we were there? Or default to current.
            // For now default to current.
            historyList->setCurrentRow(0);
        }
    });
}

void ChatWidget::onSessionClicked(QListWidgetItem* item)
{
    QString sessionId = item->data(Qt::UserRole).toString();
    
    if (sessionId.isEmpty()) {
        // Current session
        // We actually need to reload active chat messages here because we might have cleared them
        // or we rely on them being in memory?
        // Since we don't store active chat history in client besides UI text, switching back 
        // implies we should probably re-fetch active messages or just clear/show empty if we assume live stream.
        // But for better UX, let's just clear and show "Live View".
        // Ideally, we'd have a sync for "Active Chat" too. 
        // For now, let's just clear.
        
        chatTextEdit->clear();
        chatInput->setEnabled(true);
        chatTextEdit->appendPlain("--- Live Session View ---\n");
        
        // Trigger a re-sync of chat messages? Or just wait for new ones.
        // In a real implementation we would fetch active messages.
        // Let's call standard Sync? No, that's heavy.
        // We will just leave it empty for now, waiting for incoming.
    } else {
        // Archived session
        chatInput->setEnabled(false);
        chatTextEdit->clear();
        chatTextEdit->appendPlain("--- Loading Archive... ---\n");
        
        HttpReqSessionContentAsync(sessionId, *(adaptixWidget->GetProfile()), [this](bool success, const QString& msg, const QJsonObject& data) {
            if (success) {
                chatTextEdit->clear();
                chatTextEdit->appendPlain("--- Archived Session View ---\n");
                QJsonArray messages = data["messages"].toArray();
                for (const auto& val : messages) {
                    QJsonObject m = val.toObject();
                    QString username = m["c_username"].toString();
                    QString content = m["c_message"].toString();
                    qint64 date = m["c_date"].toVariant().toLongLong();
                    
                    // Manually append because AddChatMessage checks for current session
                    bool isDarkIce = (GlobalClient->settings->data.MainTheme == "Dark_Ice");
                    bool isGlass = (GlobalClient->settings->data.MainTheme == "Glass_Morph");
                    bool isHackerTech = (GlobalClient->settings->data.MainTheme == "Hacker_Tech");
                    chatTextEdit->appendColor(UnixTimestampGlobalToStringLocal(date), QColor(COLOR_Gray));
                    chatTextEdit->appendPlain(" [");
                    if (username == adaptixWidget->GetProfile()->GetUsername())
                        chatTextEdit->appendColor(username, (isDarkIce || isGlass || isHackerTech) ? QColor(COLOR_IceBlue) : QColor(COLOR_NeonGreen));
                    else
                        chatTextEdit->appendColor(username, isDarkIce ? QColor("#00C2FF") : (isHackerTech ? QColor("#00F0FF") : QColor(COLOR_KellyGreen)));
                    chatTextEdit->appendPlain("] :: ");
                    chatTextEdit->appendColor(content, isDarkIce ? QColor(COLOR_IceBlue) : (isHackerTech ? QColor("#B6FFCC") : QColor(COLOR_ConsoleWhite)));
                    chatTextEdit->appendPlain("\n");
                }
            } else {
                chatTextEdit->appendPlain("Error loading archive: " + msg);
            }
        });
    }
}

void ChatWidget::onHistoryContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = historyList->itemAt(pos);
    if (!item) return;

    QString sessionId = item->data(Qt::UserRole).toString();
    if (sessionId.isEmpty()) return; // Don't delete "Current Session"

    QMenu menu(this);
    QAction* deleteAction = menu.addAction("Delete Session");
    connect(deleteAction, &QAction::triggered, [this, item, sessionId]() {
        // Confirm deletion? For now, direct delete as requested or maybe just simple.
        // It's better to be quick for this user.
        
        HttpReqSessionDeleteAsync(sessionId, *(adaptixWidget->GetProfile()), [this, item, sessionId](bool success, const QString& message, const QJsonObject& data) {
            if (success) {
                LogInfo("Session deleted: %s", sessionId.toUtf8().constData());
                // Remove from list
                // Find item again to be safe or just use row
                int row = historyList->row(item);
                if (row >= 0) {
                    delete historyList->takeItem(row);
                }
                
                // If we deleted the currently viewed session, switch to Current
                // Check if remaining selected
                if (historyList->selectedItems().isEmpty()) {
                    historyList->setCurrentRow(0); // Current Session
                    onSessionClicked(historyList->item(0));
                }
            } else {
                MessageError("Failed to delete session: " + message);
            }
        });
    });
    
    menu.exec(historyList->mapToGlobal(pos));
}