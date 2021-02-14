#include "uimessagehandler.h"

#include <iostream>
#include <fstream>

#include "QtMessageHandler"
#include "QLoggingCategory"
#include "QTime"

#include "wpmutils.h"
#include "mainwindow.h"

QtMessageHandler oldMessageHandler = nullptr;
std::vector<QString> logMessages;
std::mutex logMutex;

void clearLogMessages()
{
    std::lock_guard<std::mutex> lock(logMutex);

    logMessages.clear();
}

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

    logMessages.push_back(s);
    if (logMessages.size() > 1000) {
        int n = logMessages.size() - 1000;
        logMessages.erase(logMessages.begin(), logMessages.begin() + n);
    }
}

