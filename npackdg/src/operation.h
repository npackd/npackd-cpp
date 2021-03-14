#ifndef OPERATION_H
#define OPERATION_H

#include <QString>

#include "job.h"

/**
 * @brief ab abstract operation that can be scheduled to a thread
 */
class Operation
{
public:
    /**
     * @brief empty
     */
    Operation();

    virtual ~Operation();

    /**
     * @brief execute the operation
     * @param job job
     * @return error message
     */
    virtual void execute(Job* job);

    /**
     * @return [move] a copy
     */
    virtual Operation* clone() const = 0;
};

#endif // OPERATION_H
