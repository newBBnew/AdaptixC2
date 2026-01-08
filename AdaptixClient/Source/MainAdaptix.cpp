#include <Workers/WebSocketWorker.h>
#include <UI/MainUI.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>
#include <Utils/TitleBarStyle.h>
#include <MainAdaptix.h>

#include <QtMath>

#include <kddockwidgets/Config.h>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();
    settings = new Settings(this);

    this->SetApplicationTheme();

    mainUI   = new MainUI();
    extender = new Extender(this);

    TitleBarStyle::applyForTheme(mainUI, settings->data.MainTheme);
}

MainAdaptix::~MainAdaptix()
{
    delete storage;
    delete mainUI;
    delete extender;
}

void MainAdaptix::Start() const
{
    QThread* ChannelThread = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    AuthProfile* authProfile = nullptr;

    while (true) {
        authProfile = this->Login();
        if (!authProfile) {
            this->Exit();
            return;
        }

        ChannelThread   = new QThread;
        ChannelWsWorker = new WebSocketWorker(authProfile);
        ChannelWsWorker->moveToThread( ChannelThread );

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        connect( ChannelWsWorker, &WebSocketWorker::connected, &loop, &QEventLoop::quit);
        connect( ChannelWsWorker, &WebSocketWorker::ws_error,  &loop, &QEventLoop::quit);
        connect( &timeoutTimer,   &QTimer::timeout,            &loop, &QEventLoop::quit);
        connect( ChannelThread,   &QThread::started, ChannelWsWorker, &WebSocketWorker::run);

        ChannelThread->start();

        timeoutTimer.start(5000);
        loop.exec();

        if (!timeoutTimer.isActive()) {
            MessageError("Server is unreachable");
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        timeoutTimer.stop();

        if (!ChannelWsWorker->ok) {
            MessageError(ChannelWsWorker->message);
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        break;
    }

    mainUI->setMinimumSize(500, 500);
    mainUI->resize(1024, 768);
    mainUI->showMaximized();
    mainUI->AddNewProject(authProfile, ChannelThread, ChannelWsWorker);

    QApplication::exec();
}

void MainAdaptix::Exit() { QCoreApplication::quit(); }

void MainAdaptix::NewProject() const
{
    QThread* ChannelThread = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    AuthProfile* authProfile = nullptr;

    while (true) {
        authProfile = this->Login();
        if (!authProfile)
            return;

        ChannelThread   = new QThread;
        ChannelWsWorker = new WebSocketWorker(authProfile);
        ChannelWsWorker->moveToThread( ChannelThread );

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        connect( ChannelWsWorker, &WebSocketWorker::connected, &loop, &QEventLoop::quit);
        connect( ChannelWsWorker, &WebSocketWorker::ws_error,  &loop, &QEventLoop::quit);
        connect( &timeoutTimer,   &QTimer::timeout,            &loop, &QEventLoop::quit);
        connect( ChannelThread,   &QThread::started, ChannelWsWorker, &WebSocketWorker::run);

        ChannelThread->start();

        timeoutTimer.start(5000);
        loop.exec();

        if (!timeoutTimer.isActive()) {
            MessageError("Server is unreachable");
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }

            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        timeoutTimer.stop();

        if (!ChannelWsWorker->ok) {
            MessageError(ChannelWsWorker->message);
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        break;
    }

    mainUI->AddNewProject(authProfile, ChannelThread, ChannelWsWorker);
}

AuthProfile* MainAdaptix::Login()
{
    AuthProfile* authProfile;
    auto dialogConnect = new DialogConnect();
    bool result;

    do {
        authProfile = dialogConnect->StartDialog();
        if ( !authProfile || !authProfile->valid)
            return NULL;

        result = HttpReqLogin( authProfile );
        if (!result)
            MessageError("Login failure");

    } while( !result );

    return authProfile;
}

void MainAdaptix::SetApplicationTheme()
{
    static bool kddwInitialized = false;
    if (!kddwInitialized) {
        KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
        KDDockWidgets::Config::self().setSeparatorThickness(5);

        auto flags = KDDockWidgets::Config::self().flags();
        flags |= KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible;
        flags |= KDDockWidgets::Config::Flag_TabsHaveCloseButton;
        flags |= KDDockWidgets::Config::Flag_ShowButtonsOnTabBarIfTitleBarHidden;
        flags |= KDDockWidgets::Config::Flag_AllowSwitchingTabsViaMenu;
        flags |= KDDockWidgets::Config::Flag_AllowReorderTabs;
        flags |= KDDockWidgets::Config::Flag_DoubleClickMaximizes;
        KDDockWidgets::Config::self().setFlags(flags);
        kddwInitialized = true;
    }

    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    FontManager::instance().initialize();

    QString appFontFamily = settings->data.FontFamily;
    if (appFontFamily.startsWith("Adaptix"))
        appFontFamily = appFontFamily.split("-")[1].trimmed();

    auto appFont = QFont( appFontFamily );
    appFont.setPointSize( settings->data.FontSize );
    QApplication::setFont( appFont );

    QString appTheme = ":/themes/" + settings->data.MainTheme;
    bool result = false;
    QString style = ReadFileString(appTheme, &result);

    QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!result || !app)
        return;

    if (settings->data.MainTheme == "Breathing_Tech") {
        breathingThemeTemplate = style;
        
        // 立即应用 QSS 结构
        app->setStyleSheet(style);

        auto applyBreathing = [this]() {
            if (settings->data.MainTheme != "Breathing_Tech") {
                if (breathingThemeTimer)
                    breathingThemeTimer->stop();
                return;
            }

            breathingThemePhase += 0.035;
            const qreal t = (qSin(breathingThemePhase) + 1.0) * 0.5;
            const qreal tPhaseOffset = (qSin(breathingThemePhase + 1.5708) + 1.0) * 0.5; // 相位超前 0.25 周期 (PI/2)

            // 1. 交互层强调色 (Highlight): 极简青蓝脉动
            const QColor accent1("#00E5FF"); // 亮青
            const QColor accent2("#0030FF"); // 深蓝
            const QColor currentAccent(
                qRound(accent1.red() + (accent2.red() - accent1.red()) * tPhaseOffset),
                qRound(accent1.green() + (accent2.green() - accent1.green()) * tPhaseOffset),
                qRound(accent1.blue() + (accent2.blue() - accent1.blue()) * tPhaseOffset)
            );

            // 2. 环境层背景同步呼吸：更深邃的灰黑基调
            const int br = 10, bg = 12, bb = 16;
            const int offset = qRound(6 * t);
            const QColor currentWindow(br + offset, bg + offset, bb + offset + 1);
            const QColor currentBase(br + offset - 2, bg + offset - 2, bb + offset);

            QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (app) {
                QPalette pal = app->palette();
                
                // 全角色同步更新
                pal.setColor(QPalette::All, QPalette::Window, currentWindow);
                pal.setColor(QPalette::All, QPalette::WindowText, QColor("#E2E8F0"));
                pal.setColor(QPalette::All, QPalette::Base, currentBase);
                pal.setColor(QPalette::All, QPalette::AlternateBase, currentWindow.lighter(108));
                pal.setColor(QPalette::All, QPalette::ToolTipBase, currentWindow);
                pal.setColor(QPalette::All, QPalette::ToolTipText, QColor("#E2E8F0"));
                pal.setColor(QPalette::All, QPalette::Text, QColor("#E2E8F0"));
                pal.setColor(QPalette::All, QPalette::Button, currentWindow.lighter(112));
                pal.setColor(QPalette::All, QPalette::ButtonText, QColor("#E2E8F0"));
                pal.setColor(QPalette::All, QPalette::BrightText, Qt::white);
                pal.setColor(QPalette::All, QPalette::Highlight, currentAccent);
                pal.setColor(QPalette::All, QPalette::HighlightedText, Qt::white);
                pal.setColor(QPalette::All, QPalette::Link, currentAccent);
                pal.setColor(QPalette::All, QPalette::PlaceholderText, QColor("#64748B"));
                
                // 3D 效果角色
                pal.setColor(QPalette::All, QPalette::Light, currentWindow.lighter(120));
                pal.setColor(QPalette::All, QPalette::Midlight, currentWindow.lighter(110));
                pal.setColor(QPalette::All, QPalette::Mid, QColor(31, 41, 55));
                pal.setColor(QPalette::All, QPalette::Dark, currentWindow.darker(120));
                pal.setColor(QPalette::All, QPalette::Shadow, Qt::black);

                app->setPalette(pal);
            }
        };

        // 初始一次性全量 Polish
        app->setStyleSheet(style);
        QPalette initPal = app->palette();
        // 确保初始状态也是正确的
        initPal.setColor(QPalette::Window, QColor(13, 17, 23));
        initPal.setColor(QPalette::Base, QColor(9, 13, 21));
        initPal.setColor(QPalette::Text, QColor("#E2E8F0"));
        app->setPalette(initPal);

        if (!breathingThemeTimer) {
            breathingThemeTimer = new QTimer(app);
            breathingThemeTimer->setInterval(60);
            breathingThemeTimer->setTimerType(Qt::CoarseTimer);
            connect(breathingThemeTimer, &QTimer::timeout, app, applyBreathing);
        }

        if (!breathingThemeTimer->isActive())
            breathingThemeTimer->start();

        applyBreathing();
        return;
    }

    if (breathingThemeTimer)
        breathingThemeTimer->stop();

    // 切换到其他非呼吸主题时，恢复标准调色板或根据主题重新设置
    app->setPalette(app->style()->standardPalette());
    app->setStyleSheet(style);
}
