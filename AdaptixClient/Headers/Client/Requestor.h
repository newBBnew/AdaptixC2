#ifndef ADAPTIXCLIENT_REQUESTOR_H
#define ADAPTIXCLIENT_REQUESTOR_H

#include <main.h>
#include <Client/HttpRequestManager.h>

class AuthProfile;

QJsonObject HttpReq(const QString &sUrl, const QByteArray &jsonData, const QString &token, int timeout = 8000 );
QJsonObject HttpReqGet(const QString &sUrl, const QString &token, int timeout = 8000 );

/// CLIENT

bool HttpReqLogin(AuthProfile* profile);

bool HttpReqJwtUpdate(AuthProfile* profile);

bool HttpReqGetOTP(const QString &type, const QString &objectId, AuthProfile profile, QString* message, bool* ok);

/// ASYNC VERSIONS

void HttpReqAgentRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetTagAsync(QStringList agentsId, const QString &tag, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetMarkAsync(QStringList agentsId, const QString &mark, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentSetColorAsync(QStringList agentsId, const QString &background, const QString &foreground, bool reset, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentUpdateDataAsync(const QString &agentId, const QJsonObject &updateData, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentGenerateAsync(const QString &listenerName, const QString &listenerType, const QString &agentName, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqAgentCommandAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqConsoleRemoveAsync(QStringList agentsId, AuthProfile& profile, HttpCallback callback);

void HttpReqTaskCancelAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksDeleteAsync(const QString &agentId, QStringList tasksId, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksHookAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTasksSaveAsync(const QString &agentId, const QString &CommandLine, int MessageType, const QString &Message, const QString &ClearText, AuthProfile& profile, HttpCallback callback);

void HttpReqCredentialsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsEditAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsRemoveAsync(const QStringList &credsId, AuthProfile& profile, HttpCallback callback);
void HttpReqCredentialsSetTagAsync(QStringList credsId, const QString &tag, AuthProfile& profile, HttpCallback callback);

void HttpReqTargetsCreateAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetEditAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetRemoveAsync(const QStringList &targetsId, AuthProfile& profile, HttpCallback callback);
void HttpReqTargetSetTagAsync(QStringList targetsId, const QString &tag, AuthProfile& profile, HttpCallback callback);

void HttpReqListenerStartAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerEditAsync(const QString &listenerName, const QString &configType, const QString &configData, AuthProfile& profile, HttpCallback callback);
void HttpReqListenerStopAsync(const QString &listenerName, const QString &listenerType, AuthProfile& profile, HttpCallback callback);

void HttpReqFileDeliveryListAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqFileDeliveryUploadAsync(const QString &fileName, const QByteArray &fileData, AuthProfile& profile, HttpCallback callback);
void HttpReqFileDeliveryDeleteAsync(const QString &fileId, AuthProfile& profile, HttpCallback callback);
void HttpReqFileDeliveryLinkCreateAsync(const QString &fileId, int expireHours, int maxUses, const QString &allowedIp, AuthProfile& profile, HttpCallback callback);

void HttpReqDownloadActionAsync(const QString &action, const QString &fileId, AuthProfile& profile, HttpCallback callback);
void HttpReqDownloadDelete(const QStringList &fileId, AuthProfile& profile, HttpCallback callback);

void HttpReqScreenSetNoteAsync(const QStringList &screensId, const QString &note, AuthProfile& profile, HttpCallback callback);
void HttpReqScreenRemoveAsync(const QStringList &screensId, AuthProfile& profile, HttpCallback callback);

void HttpReqTunnelStartServerAsync(const QString &tunnelType, const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelStopAsync(const QString &tunnelId, AuthProfile& profile, HttpCallback callback);
void HttpReqTunnelSetInfoAsync(const QString &tunnelId, const QString &info, AuthProfile& profile, HttpCallback callback);

void HttpReqChatSendMessageAsync(const QString &text, AuthProfile& profile, HttpCallback callback);

void HttpReqSessionArchiveAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqSessionDeleteAsync(const QString& sessionId, AuthProfile& profile, HttpCallback callback);
void HttpReqSessionListAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqSessionContentAsync(const QString& sessionId, AuthProfile& profile, HttpCallback callback);

void HttpReqTacticalWorkflowUpdateAsync(const QByteArray &jsonData, AuthProfile& profile, HttpCallback callback);
void HttpReqTacticalWorkflowClearAsync(AuthProfile& profile, HttpCallback callback);

void HttpReqTacticalSuggestionSendAsync(const QString &content, AuthProfile& profile, HttpCallback callback);

/// MSF API Functions

void HttpReqMSFStartAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFStopAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFStatusAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFConsoleCreateAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFConsoleWriteAsync(const QString& consoleId, const QString& command, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFConsoleReadAsync(const QString& consoleId, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFConsoleDestroyAsync(const QString& consoleId, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFSessionsListAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFSessionInteractAsync(const QString& sessionId, const QString& command, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFSessionKillAsync(const QString& sessionId, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFJobsListAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFJobKillAsync(const QString& jobId, AuthProfile& profile, HttpCallback callback);
void HttpReqMSFControllerStartAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFControllerStopAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFControllerStatusAsync(AuthProfile& profile, HttpCallback callback);
void HttpReqMSFConfigAsync(const QString& host, int port, const QString& user, const QString& password, bool ssl, AuthProfile& profile, HttpCallback callback);

#endif
