#include "eventlogmessagehandler.h"

#include <iostream>
#include <fstream>

#include "QtMessageHandler"
#include "QMutex"
#include "QLoggingCategory"
#include "QTime"

#include "wpmutils.h"

#ifdef QT_GUI_LIB
#include "mainwindow.h"
#endif

QtMessageHandler oldMessageHandler = nullptr;
QStringList logMessages;
QMutex logMutex;

void clearLogMessages()
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    logMessages.clear();
}

QStringList getLogMessages()
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    QStringList copy;
    copy.append(logMessages);
    return copy;
}

void eventLogMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    QMutexLocker lock(&logMutex);

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

#ifdef QT_GUI_LIB
    QTime time = QTime::currentTime();

    if (type == QtWarningMsg && strcmp("npackd.important", context.category) == 0) {
        QMetaObject::invokeMethod(MainWindow::getInstance(), "on_errorMessage",
                Qt::QueuedConnection,
                Q_ARG(QString, message),
                Q_ARG(QString, message), Q_ARG(bool, true),
                Q_ARG(QMessageBox::Icon, QMessageBox::Warning));
    }

    QString s = time.toString("hh:mm:ss.zzz ");
    s.append(message);

    logMessages.append(s);
    if (logMessages.size() > 1000) {
        int n = logMessages.size() - 1000;
        logMessages.erase(logMessages.begin(), logMessages.begin() + n);
    }
#else
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
#endif
}

