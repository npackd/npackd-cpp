#include <cmath>

#include "clprogress.h"
#include "wpmutils.h"

CLProgress::CLProgress(QObject *parent) :
    QObject(parent), updateRate(5), progressPos(), lastJobChange(0)
{
}


void CLProgress::jobChanged(Job* s)
{
    HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    time_t now = time(nullptr);
    if (!s->isCompleted()) {
        if (now - this->lastJobChange >= this->updateRate) {
            this->lastJobChange = now;
            if (!WPMUtils::isOutputRedirected(true)) {
                int w = progressPos.dwSize.X - 6;

                SetConsoleCursorPosition(hOutputHandle,
                        progressPos.dwCursorPosition);
                QString txt = s->getTitle();
                if (txt.length() >= w)
                    txt = "..." + txt.right(w - 3);
                if (txt.length() < w)
                    txt = txt + QString().fill(' ', w - txt.length());
                txt += QString("%1%").arg(
                        floor(s->getProgress() * 100 + 0.5), 4);
                WPMUtils::outputTextConsole(txt);
            } else {
                WPMUtils::writeln(s->getTitle());
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

void CLProgress::jobChangedSimple(Job* s)
{
    bool output = false;
    time_t now = time(nullptr);
    if (now - this->lastJobChange >= this->updateRate) {
        this->lastJobChange = now;

        output = true;
    }

    if (output) {
        double progress = s->getTopJob()->getProgress();

        int n = 0;
        QString title = s->getFullTitle();

        while (this->lastHint.length() > n && title.length() > n &&
                this->lastHint.at(n) == title.at(n)) {
            n++;
        }
        if (n) {
            int pos = title.lastIndexOf('/', n - 1);
            if (pos < 0)
                n = 0;
            else
                n = pos + 1;
        }

        QString hint;
        if (n == 0)
            hint = title;
        else {
            hint = "... " + title.mid(n);
        }

        WPMUtils::writeln(("[%1%] - " + hint).
                arg(floor(progress * 100 + 0.5)));

        this->lastHint = title;
    }
}

Job* CLProgress::createJob()
{
    HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hOutputHandle, &progressPos);
    if (progressPos.dwCursorPosition.Y >= progressPos.dwSize.Y - 1) {
        // WPMUtils::outputTextConsole("\r\n");
        progressPos.dwCursorPosition.Y--;
    }

    Job* job = new Job();
    connect(job, SIGNAL(changed(Job*)), this,
            SLOT(jobChangedSimple(Job*)));

    // -updateRate so that we do not have the initial delay
    this->lastJobChange = time(nullptr) - this->updateRate;

    return job;
}

void CLProgress::setUpdateRate(int rate)
{
    this->updateRate = rate;
}

