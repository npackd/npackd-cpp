#ifndef HRTIMER_H
#define HRTIMER_H

#include <windows.h>

/**
 * High resolution timer.
 */
class HRTimer
{
private:
    LONGLONG frequency;
    LONGLONG lastMeasurement;
    LONGLONG* durations;
    int size;
    int cur;
public:
    /**
     * Creates a new timer.
     *
     * @param size number of measuring points
     */
    HRTimer(int size);

    /**
     * @param point measuring point index (0, 1, 2, ..., size - 1)
     */
    void time(int point);

    /**
     * Prints the results
     */
    void dump() const;

    ~HRTimer();
};

#endif // HRTIMER_H
