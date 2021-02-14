#ifndef EVENTLOGMESSAGEHANDLER_H
#define EVENTLOGMESSAGEHANDLER_H

#include <vector>
#include <mutex>

#include "QString"

extern QtMessageHandler oldMessageHandler;
extern std::vector<QString> logMessages;
extern std::mutex logMutex;

void clearLogMessages();

/**
 * @brief logs to the Windows event log and to a string list
 * @param type message type
 * @param context context
 * @param message message
 */
void eventLogMessageHandler(QtMsgType type, const QMessageLogContext& context,const QString& message);

#endif // EVENTLOGMESSAGEHANDLER_H
