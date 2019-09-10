#ifndef APP_H
#define APP_H

#include "commandline.h"
#include "clprogress.h"

class App
{
private:
    CommandLine cl;
    CLProgress clp;

    int help();
    int addPath();
    int removePath();
    int listMSI();
    int getProductCode();
    int wait();
    int remove();
    void unwrapDir(Job *job);
public:
    App();

    /**
     * Process the command line.
     *
     * @return exit code
     */
    int process();
};

#endif // APP_H
