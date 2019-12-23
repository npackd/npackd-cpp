#include <QtCore/QCoreApplication>
#include <QLoggingCategory>

#include "app.h"
#include "commandlinemessagehandler.h"

int main(int /*argc*/, char */*argv*/[])
{
    oldMessageHandler = qInstallMessageHandler(eventLogMessageHandler);

    QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

    App app;

    return app.process();
}
