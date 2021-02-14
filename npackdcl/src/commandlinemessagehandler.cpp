#include "uimessagehandler.h"

#include <iostream>
#include <fstream>
#include <mutex>

#include "QtMessageHandler"
#include "QLoggingCategory"
#include "QTime"

#include "wpmutils.h"

QtMessageHandler oldMessageHandler = nullptr;
std::mutex logMutex;

void eventLogMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!npackd().isEnabled(type))
        return;

    // event log is too slow for debug messages
    if (type != QtDebugMsg) {
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

    switch (type) {
        case QtWarningMsg:
            WPMUtils::writeln("WARNING: " + message, true);
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            WPMUtils::outputTextConsole("ERROR: " + message + QStringLiteral("\r\n"), false, true,
                    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
                    FOREGROUND_INTENSITY | BACKGROUND_RED);
            break;
        case QtInfoMsg:
            if (strcmp("npackd.important", context.category) == 0) {
                WPMUtils::outputTextConsole(message + QStringLiteral("\r\n"), true, true,
                        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
                        FOREGROUND_INTENSITY | BACKGROUND_GREEN);
            } else {
                WPMUtils::writeln(message, true);
            }
            break;
        default:
            WPMUtils::writeln(message, true);
    }
}

