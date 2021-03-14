#include "controlservicesoperation.h"

ControlServicesOperation::ControlServicesOperation(bool start): start(start)
{

}

void ControlServicesOperation::execute(Job *job)
{
    // TODO
    job->complete();
}

ControlServicesOperation *ControlServicesOperation::clone() const
{
    return new ControlServicesOperation(*this);
}
