#ifndef EVENTLOGMESSAGEHANDLER_H
#define EVENTLOGMESSAGEHANDLER_H

#include "QString"

/**
 * @brief logs to the Windows event log
 * @param type message type
 * @param context context
 * @param message message
 */
void eventLogMessageHandler(QtMsgType type, const QMessageLogContext& context,const QString& message);

#endif // EVENTLOGMESSAGEHANDLER_H
