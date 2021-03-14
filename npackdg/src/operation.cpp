#include "operation.h"

Operation::Operation()
{

}

Operation::~Operation()
{

}

void Operation::execute(Job *job)
{
    job->complete();
}
