#ifndef APP_H
#define APP_H

#include <QObject>
#include <QtTest/QtTest>

class App: public QObject
{
    Q_OBJECT
private:
    QString captureNpackdCLOutput(const QString &params);
public:
    App();
private slots:
    /**
     * Functional tests
     */
    void functionalTest();

    /**
     * @brief "npackdcl path" must be fast
     */
    void pathIsFast();
};

#endif // APP_H
