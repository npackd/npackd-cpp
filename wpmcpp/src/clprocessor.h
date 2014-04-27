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
     * @brief installs a package
     * @param cl command line
     * @return error message or ""
     */
    QString add(const CommandLine &cl);

    /**
     * @brief removes a package
     * @param cl command line
     * @return error message or ""
     */
    QString remove(const CommandLine &cl);
private:
    void monitor(Job *job, const QString &title, QThread *thread);
};

#endif // CLPROCESSOR_H
