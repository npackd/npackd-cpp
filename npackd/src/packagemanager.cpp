#include "packagemanager.h"

PackageManager PackageManager::def;

PackageManager* PackageManager::getDefault()
{
    return &def;
}

PackageManager::PackageManager()
{
}

PackageManager::~PackageManager()
{
}
