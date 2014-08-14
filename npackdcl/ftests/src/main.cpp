#include <QCoreApplication>
#include <QTimer>

#include "app.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    App app;

    QTimer::singleShot(0, &app, SLOT(functionalTest()));

    return a.exec();
}
