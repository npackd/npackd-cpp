#include <QtCore/QCoreApplication>

#include "app.h"
#include "commandlinemessagehandler.h"

int main(int /*argc*/, char */*argv*/[])
{
    oldMessageHandler = qInstallMessageHandler(eventLogMessageHandler);

    App app;

    return app.process();
}
