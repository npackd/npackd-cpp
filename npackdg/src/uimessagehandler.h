#ifndef EVENTLOGMESSAGEHANDLER_H
#define EVENTLOGMESSAGEHANDLER_H

#include "QString"
#include "QStringList"
#include "QMutex"

extern QtMessageHandler oldMessageHandler;
extern QStringList logMessages;
extern QMutex logMutex;

void clearLogMessages();

QStringList getLogMessages();

/**
 * @brief logs to the Windows event log and to a QStringList
 * @param type message type
 * @param context context
 * @param message message
 */
void eventLogMessageHandler(QtMsgType type, const QMessageLogContext& context,const QString& message);

#endif // EVENTLOGMESSAGEHANDLER_H
