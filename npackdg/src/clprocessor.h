#ifndef CLPROCESSOR_H
#define CLPROCESSOR_H

#include <QString>

#include "commandline.h"
#include "job.h"
#include "installoperation.h"

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

    ~CLProcessor();

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
     * @param arg number of arguments
     * @param argv program arguments
     * @param errorCode error code will be stored here
     * @return false = GUI should be started
     */
    bool process(int argc, char *argv[], int *errorCode);
private:
    /**
     * @param install the objects will be destroyed
     * @param opsDependencies dependencies between installation operations. The
     *     nodes of the graph are indexes in the "ops" vector. An edge from "u"
     *     to "v" means that "u" depends on "v".
     * @param programCloseType how the programs should be closed
     * @return error message or ""
     */
    QString process(std::vector<InstallOperation *> &install,
            const DAG &opsDependencies, DWORD programCloseType);

    /**
     * @brief waits for a job
     * @param job the object will be destroyed
     */
    void monitorAndWaitFor(Job *job);

    QString startNewestNpackdg();

    QString checkForUpdates();
};

#endif // CLPROCESSOR_H
