#include <Client/CommandSubmitter.h>

#include <Agent/Agent.h>
#include <Agent/Commander.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <UI/Dialogs/DialogUploader.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

namespace {

struct PendingTaskIdResolve {
    TaskIdCallback cb;
    qint64 createdAt = 0;
};

QMutex g_pendingTaskIdMu;
QMap<QString, PendingTaskIdResolve> g_pendingTaskId;

QString generateUniqueHandlerId(AdaptixWidget* adaptixWidget)
{
    QString id = GenerateRandomString(8, "hex");
    for (;;) {
        bool collision = false;
        if (adaptixWidget && adaptixWidget->PostHandlersJS.contains(id))
            collision = true;
        {
            QMutexLocker lock(&g_pendingTaskIdMu);
            if (g_pendingTaskId.contains(id))
                collision = true;
        }
        if (!collision)
            break;
        id = GenerateRandomString(8, "hex");
    }
    return id;
}

void registerTaskIdCallback(const QString& handlerId, TaskIdCallback cb)
{
    if (handlerId.isEmpty() || !cb)
        return;

    QMutexLocker lock(&g_pendingTaskIdMu);
    g_pendingTaskId[handlerId] = {cb, QDateTime::currentSecsSinceEpoch()};
}

void removeTaskIdCallback(const QString& handlerId)
{
    if (handlerId.isEmpty())
        return;

    QMutexLocker lock(&g_pendingTaskIdMu);
    g_pendingTaskId.remove(handlerId);
}
}

void CommandSubmitter::Submit(AdaptixWidget* adaptixWidget,
                              Agent* agent,
                              const QString& commandLine,
                              const CommanderResult& cmdResult,
                              const bool ui,
                              QWidget* uploadParent,
                              const bool showErrors,
                              CommandSubmitCallback callback,
                              TaskIdCallback taskIdCallback)
{
    CommandSubmitInfo info;

    if (!adaptixWidget || !agent || !agent->adaptixWidget || !agent->adaptixWidget->GetProfile()) {
        info.ok = false;
        info.message = "Invalid context";
        if (callback)
            callback(info);
        return;
    }

    QString hookId = "";
    if (cmdResult.post_hook.isSet) {
        hookId = GenerateRandomString(8, "hex");
        while (adaptixWidget->PostHooksJS.contains(hookId))
            hookId = GenerateRandomString(8, "hex");

        adaptixWidget->PostHooksJS[hookId] = cmdResult.post_hook;
    }

    QString handlerId = "";
    if (cmdResult.handler.isSet) {
        handlerId = generateUniqueHandlerId(adaptixWidget);

        adaptixWidget->PostHandlersJS[handlerId] = cmdResult.handler;
    }
    else if (taskIdCallback) {
        handlerId = generateUniqueHandlerId(adaptixWidget);
    }

    QJsonDocument jsonDoc(cmdResult.data);
    QString commandData = jsonDoc.toJson();

    QJsonObject dataJson;
    dataJson["name"]          = agent->data.Name;
    dataJson["id"]            = agent->data.Id;
    dataJson["ui"]            = ui;
    dataJson["cmdline"]       = commandLine;
    dataJson["data"]          = commandData;
    dataJson["ax_hook_id"]    = hookId;
    dataJson["ax_handler_id"] = handlerId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    info.hookId = hookId;
    info.handlerId = handlerId;
    info.taskId = "";

    registerTaskIdCallback(handlerId, taskIdCallback);

    /// 5 Mb
    if (commandData.size() < 0x500000) {
        HttpReqAgentCommandAsync(jsonData, *(agent->adaptixWidget->GetProfile()),
                                 [adaptixWidget, cmdResult, hookId, handlerId, callback](bool success, const QString &message, const QJsonObject& response) {
            CommandSubmitInfo cbInfo;
            cbInfo.ok = success;
            cbInfo.usedLargePayload = false;
            cbInfo.message = message;
            cbInfo.hookId = hookId;
            cbInfo.handlerId = handlerId;
            cbInfo.taskId = "";

            if (success && response.contains("task_id")) {
                cbInfo.taskId = response["task_id"].toString();
                if (!handlerId.isEmpty() && !cbInfo.taskId.isEmpty())
                    CommandSubmitter::ResolveTaskId(handlerId, cbInfo.taskId);
            }

            if (!success) {
                if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                    adaptixWidget->PostHooksJS.remove(hookId);
                if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
                    adaptixWidget->PostHandlersJS.remove(handlerId);

                removeTaskIdCallback(handlerId);
            }

            if (callback)
                callback(cbInfo);
        });
        return;
    }

    /// 1. Get OTP

    QString message = QString();
    bool ok = false;
    QString objId = GenerateRandomString(8, "hex");
    bool result = HttpReqGetOTP("tmp_upload", objId, *(agent->adaptixWidget->GetProfile()), &message, &ok);
    if (!result) {
        if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
            adaptixWidget->PostHooksJS.remove(hookId);
        if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
            adaptixWidget->PostHandlersJS.remove(handlerId);
        if (showErrors)
            MessageError("Response timeout");

        info.ok = false;
        info.usedLargePayload = true;
        info.message = "Response timeout";
        removeTaskIdCallback(handlerId);
        if (callback)
            callback(info);
        return;
    }
    if (!ok) {
        if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
            adaptixWidget->PostHooksJS.remove(hookId);
        if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
            adaptixWidget->PostHandlersJS.remove(handlerId);
        if (showErrors)
            MessageError(message);

        info.ok = false;
        info.usedLargePayload = true;
        info.message = message;
        removeTaskIdCallback(handlerId);
        if (callback)
            callback(info);
        return;
    }
    QString otp = message;

    /// 2. Upload with OTP

    QString sUrl = agent->adaptixWidget->GetProfile()->GetURL() + "/otp/upload/temp";

    auto* uploaderDialog = new DialogUploader(sUrl, otp, jsonData, uploadParent);
    uploaderDialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject* contextObject = uploadParent ? static_cast<QObject*>(uploadParent) : static_cast<QObject*>(uploaderDialog);
    QObject::connect(uploaderDialog, &DialogUploader::finished, contextObject, [adaptixWidget, cmdResult, hookId, handlerId, objId, agent, callback, showErrors](const bool success) {
        CommandSubmitInfo cbInfo;
        cbInfo.ok = success;
        cbInfo.usedLargePayload = true;
        cbInfo.message = success ? "" : "Upload failed";
        cbInfo.hookId = hookId;
        cbInfo.handlerId = handlerId;
        cbInfo.taskId = "";

        if (!success) {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
                adaptixWidget->PostHandlersJS.remove(handlerId);
            removeTaskIdCallback(handlerId);
            if (callback)
                callback(cbInfo);
            return;
        }

        /// 3. Send Command

        QJsonObject data2Json;
        data2Json["object_id"] = objId;
        QByteArray json2Data = QJsonDocument(data2Json).toJson();

        QString sUrl2 = agent->adaptixWidget->GetProfile()->GetURL() + "/agent/command/file";
        QJsonObject jsonObject = HttpReq(sUrl2, json2Data, agent->adaptixWidget->GetProfile()->GetAccessToken(), 0);
        if (jsonObject.contains("message") && jsonObject.contains("ok")) {
            if (jsonObject["ok"].toBool() == false) {
                if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                    adaptixWidget->PostHooksJS.remove(hookId);
                if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
                    adaptixWidget->PostHandlersJS.remove(handlerId);

                cbInfo.ok = false;
                cbInfo.message = jsonObject["message"].toString();
                removeTaskIdCallback(handlerId);
                if (showErrors)
                    MessageError(cbInfo.message);
                if (callback)
                    callback(cbInfo);
            } else {
                if (jsonObject.contains("task_id")) {
                    cbInfo.taskId = jsonObject["task_id"].toString();
                    if (!handlerId.isEmpty() && !cbInfo.taskId.isEmpty())
                        CommandSubmitter::ResolveTaskId(handlerId, cbInfo.taskId);
                }
                if (callback)
                    callback(cbInfo);
            }
        } else {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            if (cmdResult.handler.isSet && adaptixWidget->PostHandlersJS.contains(handlerId))
                adaptixWidget->PostHandlersJS.remove(handlerId);

            cbInfo.ok = false;
            cbInfo.message = "Response timeout";
            removeTaskIdCallback(handlerId);
            if (showErrors)
                MessageError(cbInfo.message);
            if (callback)
                callback(cbInfo);
        }
    });

    uploaderDialog->exec();
}

void CommandSubmitter::ResolveTaskId(const QString& handlerId, const QString& taskId)
{
    if (handlerId.isEmpty() || taskId.isEmpty())
        return;

    PendingTaskIdResolve pending;
    {
        QMutexLocker lock(&g_pendingTaskIdMu);
        if (!g_pendingTaskId.contains(handlerId))
            return;
        pending = g_pendingTaskId.take(handlerId);
    }

    if (pending.cb)
        pending.cb(handlerId, taskId);
}
