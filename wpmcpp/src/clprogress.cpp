#include <math.h>

#include "clprogress.h"
#include "wpmutils.h"

CLProgress::CLProgress(QObject *parent) :
    QObject(parent), updateRate(5)
{
}

void CLProgress::jobChanged(const JobState& s)
{
    HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    time_t now = time(0);
    if (!s.completed) {
        if (now - this->lastJobChange >= this->updateRate) {
            this->lastJobChange = now;
            if (!WPMUtils::isOutputRedirected(true)) {
                int w = progressPos.dwSize.X - 6;

                SetConsoleCursorPosition(hOutputHandle,
                        progressPos.dwCursorPosition);
                QString txt = s.hint;
                if (txt.length() >= w)
                    txt = "..." + txt.right(w - 3);
                if (txt.length() < w)
                    txt = txt + QString().fill(' ', w - txt.length());
                txt += QString("%1%").arg(floor(s.progress * 100 + 0.5), 4);
                WPMUtils::outputTextConsole(txt);
            } else {
                WPMUtils::outputTextConsole(s.hint + "\n");
            }
        }
    } else {
        if (!WPMUtils::isOutputRedirected(true)) {
            QString filled;
            filled.fill(' ', progressPos.dwSize.X - 1);
            SetConsoleCursorPosition(hOutputHandle,
                    progressPos.dwCursorPosition);
            WPMUtils::outputTextConsole(filled);
            SetConsoleCursorPosition(hOutputHandle,
                    progressPos.dwCursorPosition);
        }
    }
}

void CLProgress::jobChangedSimple(const JobState& s)
{
    bool output = false;
    if (!s.completed) {
        time_t now = time(0);
        if (now - this->lastJobChange >= this->updateRate) {
            this->lastJobChange = now;

            output = true;
        }
    } else {
        output = true;
    }

    if (output) {
        int n = 0;
        while (this->lastHint.length() > n && s.hint.length() > n &&
                this->lastHint.at(n) == s.hint.at(n)) {
            n++;
        }
        if (n) {
            int pos = s.hint.lastIndexOf('/', n - 1);
            if (pos < 0)
                n = 0;
            else
                n = pos + 1;
        }

        QString hint;
        if (n == 0)
            hint = s.hint;
        else {
            hint = "... " + s.hint.mid(n);
        }

        WPMUtils::outputTextConsole(("[%1%] - " + hint + "\n").
                arg(floor(s.progress * 100 + 0.5)));

        this->lastHint = s.hint;
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

