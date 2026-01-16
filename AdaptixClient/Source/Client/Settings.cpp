#include <UI/Dialogs/DialogSettings.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>

Settings::Settings(MainAdaptix* m)
{
    mainAdaptix = m;

    this->SetDefault();
    this->LoadFromDB();
}

Settings::~Settings() = default;

MainAdaptix* Settings::getMainAdaptix()
{
    return this->mainAdaptix;
}

DialogSettings* Settings::getDialogSettings()
{
    if (!dialogSettings) {
        dialogSettings = new DialogSettings(this);
    }
    return dialogSettings;
}

void Settings::SetDefault()
{
    this->data.MainTheme    = "Dark";
    this->data.FontFamily   = "AO - DejaVu Sans Mono";
    this->data.FontSize     = 10;
    this->data.GraphVersion = "Version 1";
    this->data.RemoteTerminalBufferSize = 10000;

    this->data.ConsoleTime = true;
    this->data.ConsoleBufferSize = 50000;
    this->data.ConsoleNoWrap = true;
    this->data.ConsoleAutoScroll = false;

    for ( int i = 0; i < 16; i++)
        data.SessionsTableColumns[i] = true;

    for ( int i = 0; i < 16; i++) {
        data.SessionsColumnOrder[i] = -1;  // -1 = default order
        data.SessionsColumnWidths[i] = -1; // -1 = auto width
    }

    this->data.CheckHealth = true;
    this->data.HealthCoaf = 2.0;
    this->data.HealthOffset = 40;

    for ( int i = 0; i < 11; i++)
        data.TasksTableColumns[i] = true;

    this->data.TabBlinkEnabled = true;

    this->data.AISystemPrompt = "You are a tactical AI assistant integrated into Always Online. Your goal is to assist operators with reconnaissance, analysis, and decision support. Be concise, professional, and focus on operational security.";

    // MSF Settings
    this->data.MSFEnabled = false;
    this->data.MSFHost = "127.0.0.1";
    this->data.MSFPort = 55553;
    this->data.MSFUser = "msf";
    this->data.MSFPassword = "test123";
    this->data.MSFSSL = true;
}

void Settings::LoadFromDB()
{
    mainAdaptix->storage->SelectSettingsMain( &data );
    mainAdaptix->storage->SelectSettingsConsole( &data );
    mainAdaptix->storage->SelectSettingsSessions( &data );
    mainAdaptix->storage->SelectSettingsGraph( &data );
    mainAdaptix->storage->SelectSettingsTasks( &data );
    mainAdaptix->storage->SelectSettingsTabBlink( &data );
    mainAdaptix->storage->SelectSettingsAI( &data );
    mainAdaptix->storage->SelectSettingsMSF( &data );
}

void Settings::SaveToDB() const
{
    mainAdaptix->storage->UpdateSettingsMain( data );
    mainAdaptix->storage->UpdateSettingsConsole( data );
    mainAdaptix->storage->UpdateSettingsSessions( data );
    mainAdaptix->storage->UpdateSettingsGraph( data );
    mainAdaptix->storage->UpdateSettingsTasks( data );
    mainAdaptix->storage->UpdateSettingsTabBlink( data );
    mainAdaptix->storage->UpdateSettingsAI( data );
    mainAdaptix->storage->UpdateSettingsMSF( data );
}
