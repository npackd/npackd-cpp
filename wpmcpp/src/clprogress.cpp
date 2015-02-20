#include <math.h>

#include "clprogress.h"
#include "wpmutils.h"

CLProgress::CLProgress(QObject *parent) :
    QObject(parent), updateRate(5), lastJobChange(-1)
{
}

void CLProgress::jobChangedSimple(const JobState& s)
{
    bool output = false;

    if (s.job->parentJob && !s.job->parentJob->parentJob) {
        time_t now = time(0);
        if (now - this->lastJobChange >= this->updateRate) {
            this->lastJobChange = now;

            output = true;
        }
    }

    if (output) {
        double progress = s.job->parentJob->getProgress();
        WPMUtils::outputTextConsole(("[%1%] - " + s.title + "\n").
                arg(floor(progress * 100 + 0.5)));
    }
}

Job* CLProgress::createJob()
{
    HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutputHandle, &progressPos);
    if (progressPos.dwCursorPosition.Y >= progressPos.dwSize.Y - 1) {
        // WPMUtils::outputTextConsole("\n");
        progressPos.dwCursorPosition.Y--;
    }

    Job* job = new Job();
    connect(job, SIGNAL(changed(const JobState&)), this,
            SLOT(jobChangedSimple(const JobState&)));

    // -updateRate so that we do not have the initial delay
    this->lastJobChange = time(0) - this->updateRate;

    return job;
}

void CLProgress::setUpdateRate(int rate)
{
    this->updateRate = rate;
}

