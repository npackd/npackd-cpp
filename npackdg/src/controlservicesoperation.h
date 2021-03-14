#ifndef CONTROLSERVICESOPERATION_H
#define CONTROLSERVICESOPERATION_H

#include <vector>

#include <QString>

#include "operation.h"

/**
 * @brief start or stop Windows services
 */
class ControlServicesOperation: public Operation
{
public:
    /**
     * @brief true = start, false = stop
     */
    bool start;

    /**
     * @brief internal Windows service names
     */
    std::vector<QString> services;

    /**
     * @brief creates an operation
     */
    ControlServicesOperation(bool start);

    void execute(Job *job) override;
    ControlServicesOperation* clone() const override;
};

#endif // CONTROLSERVICESOPERATION_H
