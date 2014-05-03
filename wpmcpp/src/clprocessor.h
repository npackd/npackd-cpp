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
public:
    /**
     * @brief -
     */
    CLProcessor();

    /**
     * @brief installs packages
     * @param cl command line
     * @return error message or ""
     */
    QString add(const CommandLine &cl);

    /**
     * @brief removes packages
     * @param cl command line
     * @return error message or ""
     */
    QString remove(const CommandLine &cl);

    /**
     * @brief updates packages
     * @param cl command line
     * @return error message or ""
     */
    QString update(const CommandLine &cl);

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
};

#endif // CLPROCESSOR_H
