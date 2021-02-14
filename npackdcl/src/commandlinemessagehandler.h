#ifndef EVENTLOGMESSAGEHANDLER_H
#define EVENTLOGMESSAGEHANDLER_H

#include <mutex>

#include "QString"

extern QtMessageHandler oldMessageHandler;
extern std::mutex logMutex;

/**
 * @brief logs to the Windows event log and to the console
 * @param type message type
 * @param context context
 * @param message message
 */
void eventLogMessageHandler(QtMsgType type,
        const QMessageLogContext& context, const QString& message);

#endif // EVENTLOGMESSAGEHANDLER_H
