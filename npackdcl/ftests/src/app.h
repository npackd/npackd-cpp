#ifndef APP_H
#define APP_H

#include <QObject>

class App: public QObject
{
    Q_OBJECT
private:
    QString captureNpackdCLOutput(const QString &params);
public:
    App();
public slots:
    /**
     * Functional tests
     *
     * @return exit code
     */
    int functionalTest();
};

#endif // APP_H
