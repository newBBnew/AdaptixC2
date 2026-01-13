#ifndef ADAPTIXCLIENT_MAINADAPTIX_H
#define ADAPTIXCLIENT_MAINADAPTIX_H

#include <main.h>

class Extender;
class Settings;
class Storage;
class MainUI;
class AuthProfile;
class WebSocketWorker;

class MainAdaptix : public QWidget {

public:
    MainUI*   mainUI   = nullptr;
    Storage*  storage  = nullptr;
    Extender* extender = nullptr;
    Settings* settings = nullptr;

    QTimer*  breathingThemeTimer = nullptr;
    QString  breathingThemeTemplate;
    qreal    breathingThemePhase = 0.0;

    explicit MainAdaptix();
    ~MainAdaptix() override;

    static void Exit();

    void Start() const;
    void NewProject() const;
    void SetApplicationTheme();

    static AuthProfile* Login();

private:
    bool ConnectToServer(AuthProfile*& outProfile, QThread*& outThread, WebSocketWorker*& outWorker) const;
};

extern MainAdaptix* GlobalClient;

#endif
