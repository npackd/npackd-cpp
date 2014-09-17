#include <QCoreApplication>
#include <QTimer>

#include "vimorgrepapp.h"
#include "job.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QCoreApplication ca(argc, argv);

    qRegisterMetaType<JobState>("JobState");
    qRegisterMetaType<Version>("Version");

    VimOrgRepApp app;

    QTimer::singleShot(0, &app, SLOT(process()));

    return ca.exec();
}
