#ifndef APP_H
#define APP_H

#include <time.h>

#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <qdebug.h>
#include <qstringlist.h>
#include <qstring.h>

#include "repository.h"
#include "commandline.h"
#include "job.h"
#include "clprogress.h"

/**
 * NpackdCL tests
 */
class App: public QObject
{
    Q_OBJECT
private slots:
    /**
     * Tests
     */
    void test();

    /**
     * Tests for InstalledPackages
     */
    void testInstalledPackages();

    /**
     * Tests for CommandLine
     */
    void testCommandLine();

    /**
     * Tests for WPMUtils::copyDirectory
     */
    void testCopyDirectory();

    /**
     * Tests for WPMUtils::normalizePath
     */
    void testNormalizePath();
};

#endif // APP_H
