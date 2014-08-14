#include "hrtimer.h"

#include <windows.h>

#include <QDebug>

HRTimer::HRTimer(int size)
{
    this->cur = 0;
    this->size = size;
    this->lastMeasurement = -1;

    this->durations = new LONGLONG[size];
    memset(this->durations, 0, sizeof(durations[0]) * size);

    LARGE_INTEGER liFrequency;
    QueryPerformanceFrequency(&liFrequency);
    frequency = liFrequency.QuadPart;
}

void HRTimer::time(int point)
{
    if (point != cur)
        qDebug() << "HRTimer: " << point << " != " << cur;

    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);

    if (lastMeasurement >= 0) {
        this->durations[cur] += v.QuadPart - lastMeasurement;
    }
    lastMeasurement = v.QuadPart;
    cur++;
    if (cur == this->size)
        cur = 0;
}

void HRTimer::dump() const
{
    LONGLONG sum = 0;
    for (int i = 0; i < this->size; i++) {
        qDebug() << i << ": " <<
                (this->durations[i] * 1000 / this->frequency) << " ms";
        sum += this->durations[i];
    }
    qDebug() << "Sum from 0 to " << this->size - 1 << ": " <<
            (sum * 1000 / this->frequency) << " ms";
}

HRTimer::~HRTimer()
{
    delete[] this->durations;
}

double HRTimer::getTime(int point)
{
    return ((double) this->durations[point]) / this->frequency;
}
