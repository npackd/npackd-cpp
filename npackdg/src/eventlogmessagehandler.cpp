#include "eventlogmessagehandler.h"

#include <iostream>
#include <fstream>

#include "QtMessageHandler"
#include "QMutex"

#include "wpmutils.h"

void eventLogMessageHandler(QtMsgType type,const QMessageLogContext& /*context*/,const QString&message) {
    // event log is too slow for debug messages
    if (type != QtDebugMsg) {
        static QMutex mutex;
        QMutexLocker lock(&mutex);

        WORD t;
        switch (type) {
            case QtWarningMsg:
                t = EVENTLOG_WARNING_TYPE;
                break;
            case QtCriticalMsg:
            case QtFatalMsg:
                t = EVENTLOG_ERROR_TYPE;
                break;
            default:
                t = EVENTLOG_INFORMATION_TYPE;
        }

        WPMUtils::reportEvent(message, t);
    }
}

