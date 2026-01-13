#ifndef ADAPTIXCLIENT_COMMANDSUBMITTER_H
#define ADAPTIXCLIENT_COMMANDSUBMITTER_H

#include <functional>
#include <main.h>

class AdaptixWidget;
class Agent;
struct CommanderResult;

struct CommandSubmitInfo {
    bool ok = false;
    bool usedLargePayload = false;
    QString message;
    QString hookId;
    QString handlerId;
    QString taskId;
};

using CommandSubmitCallback = std::function<void(const CommandSubmitInfo& info)>;
using TaskIdCallback = std::function<void(const QString& handlerId, const QString& taskId)>;

class CommandSubmitter
{
public:
    static void Submit(AdaptixWidget* adaptixWidget,
                       Agent* agent,
                       const QString& commandLine,
                       const CommanderResult& cmdResult,
                       bool ui,
                       QWidget* uploadParent,
                       bool showErrors,
                       CommandSubmitCallback callback,
                       TaskIdCallback taskIdCallback = nullptr);

    static void ResolveTaskId(const QString& handlerId, const QString& taskId);
};

#endif
