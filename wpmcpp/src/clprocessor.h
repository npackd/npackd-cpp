#ifndef CLPROCESSOR_H
#define CLPROCESSOR_H

#include <QString>
#include <QThread>

#include "commandline.h"
#include "job.h"

/**
 * @brief process the command line
 */
class CLProcessor
{
    CommandLine cl;
public:
    /**
     * @brief -
     */
    CLProcessor();

    /**
     * @brief installs packages
     * @return error message or ""
     */
    QString add();

    /**
     * @brief removes packages
     * @return error message or ""
     */
    QString remove();

    /**
     * @brief updates packages
     * @return error message or ""
     */
    QString update();

    /**
     * @brief show the command line options help
     */
    void usage();

    /**
     * @brief process the command line
     * @param errorCode error code will be stored here
     * @return false = GUI should be started
     */
    bool process(int *errorCode);
private:
    void monitor(Job *job, const QString &title, QThread *thread);
    QString startNewestNpackdg();
};

#endif // CLPROCESSOR_H
