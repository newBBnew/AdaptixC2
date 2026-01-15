#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>
#include <QPlainTextEdit>

class Settings;

class DialogSettings : public QWidget
{
Q_OBJECT

    Settings* settings = nullptr;

    QGridLayout*    layoutMain    = nullptr;
    QListWidget*    listSettings  = nullptr;
    QVBoxLayout*    headerLayout  = nullptr;
    QLabel*         labelHeader   = nullptr;
    QFrame*         lineFrame     = nullptr;
    QStackedWidget* stackSettings = nullptr;
    QSpacerItem*    hSpacer       = nullptr;
    QPushButton*    buttonApply   = nullptr;
    QPushButton*    buttonClose   = nullptr;

    QWidget*     mainSettingWidget = nullptr;
    QGridLayout* mainSettingLayout = nullptr;
    QLabel*      themeLabel        = nullptr;
    QComboBox*   themeCombo        = nullptr;
    QLabel*      fontSizeLabel     = nullptr;
    QSpinBox*    fontSizeSpin      = nullptr;
    QLabel*      fontFamilyLabel   = nullptr;
    QComboBox*   fontFamilyCombo   = nullptr;
    QLabel*      graphLabel1       = nullptr;
    QComboBox*   graphCombo1       = nullptr;
    QLabel*      terminalSizeLabel = nullptr;
    QSpinBox*    terminalSizeSpin  = nullptr;

    QGroupBox*   consoleGroup              = nullptr;
    QGridLayout* consoleGroupLayout        = nullptr;
    QLabel*      consoleSizeLabel          = nullptr;
    QSpinBox*    consoleSizeSpin           = nullptr;
    QCheckBox*   consoleTimeCheckbox       = nullptr;
    QCheckBox*   consoleNoWrapCheckbox     = nullptr;
    QCheckBox*   consoleAutoScrollCheckbox = nullptr;

    QWidget*     sessionsWidget       = nullptr;
    QGridLayout* sessionsLayout       = nullptr;
    QGroupBox*   sessionsGroup        = nullptr;
    QGridLayout* sessionsGroupLayout  = nullptr;
    int          sessionsCheckCount   = 16;
    QCheckBox*   sessionsCheck[16];
    QCheckBox*   sessionsHealthCheck  = nullptr;
    QLabel*      sessionsLabel1       = nullptr;
    QLabel*      sessionsLabel2       = nullptr;
    QLabel*      sessionsLabel3       = nullptr;
    QDoubleSpinBox* sessionsCoafSpin  = nullptr;
    QSpinBox*    sessionsOffsetSpin   = nullptr;

    QWidget*     tasksWidget      = nullptr;
    QGridLayout* tasksLayout      = nullptr;
    QGroupBox*   tasksGroup       = nullptr;
    QGridLayout* tasksGroupLayout = nullptr;
    QCheckBox*   tasksCheck[11];

    QWidget*     tabblinkWidget          = nullptr;
    QGridLayout* tabblinkLayout          = nullptr;
    QCheckBox*   tabblinkEnabledCheckbox = nullptr;
    QGroupBox*   tabblinkGroup           = nullptr;
    QGridLayout* tabblinkGroupLayout     = nullptr;
    QMap<QString, QCheckBox*> m_tabblinkChecks;  // className -> checkbox

    QWidget*        aiWidget         = nullptr;
    QGridLayout*    aiLayout         = nullptr;
    QLabel*         aiPromptLabel    = nullptr;
    QPlainTextEdit* aiPromptTextEdit = nullptr;

    // MSF Settings
    QWidget*        msfWidget       = nullptr;
    QGridLayout*    msfLayout       = nullptr;
    QCheckBox*      msfEnabledCheck = nullptr;
    QLabel*         msfHostLabel    = nullptr;
    QLineEdit*      msfHostEdit     = nullptr;
    QLabel*         msfPortLabel    = nullptr;
    QSpinBox*       msfPortSpin     = nullptr;
    QLabel*         msfUserLabel    = nullptr;
    QLineEdit*      msfUserEdit     = nullptr;
    QLabel*         msfPassLabel    = nullptr;
    QLineEdit*      msfPassEdit     = nullptr;
    QCheckBox*      msfSSLCheck     = nullptr;

    void createUI();
    void loadSettings();

protected:
    void showEvent(QShowEvent* event) override;

public:
    DialogSettings(Settings* s);

public Q_SLOTS:
    void onStackChange(int index) const;
    void onHealthChange() const;
    void onBlinkChange() const;
    void onApply() const;
    void onClose();
};

#endif