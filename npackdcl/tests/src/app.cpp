#include <limits>
#include <math.h>

#include <QRegExp>
#include <QScopedPointer>
#include <QProcess>

#include "app.h"
#include "wpmutils.h"
#include "commandline.h"
#include "downloader.h"
#include "installedpackages.h"
#include "installedpackageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "hrtimer.h"

void App::test()
{
    Version a;
    Version b;
    b.setVersion("1.0.0");
    QVERIFY(a == b);

    a.setVersion("4.5.6.7.8.9.10");
    QVERIFY(a.getVersionString() == "4.5.6.7.8.9.10");

    a.setVersion("1.1");
    QVERIFY(a == Version(1, 1));

    a.setVersion("5.0.0.1");
    Version c(a);
    QVERIFY(c.getVersionString() == "5.0.0.1");

    Version* d = new Version();
    d->setVersion(7, 8, 9, 10);
    delete d;

    a.setVersion(1, 17);
    QVERIFY(a.getVersionString() == "1.17");

    a.setVersion(2, 18, 3);
    QVERIFY(a.getVersionString() == "2.18.3");

    a.setVersion(3, 1, 3, 8);
    QVERIFY(a.getVersionString() == "3.1.3.8");

    a.setVersion("17.2.8.4");
    a.prepend(8);
    a.prepend(38);
    a.prepend(0);
    QVERIFY(a.getVersionString() == "0.38.8.17.2.8.4");

    a.setVersion("2.8.3");
    QVERIFY(a.getVersionString(7) == "2.8.3.0.0.0.0");

    a.setVersion("17.2");
    QVERIFY(a.getNParts() == 2);

    a.setVersion("8.4.0.0.0");
    a.normalize();
    QVERIFY(a.getVersionString() == "8.4");
    QVERIFY(a.isNormalized());

    QVERIFY(Version("2.8.7.4.8.9") > Version("2.8.6.4.8.8"));
}
