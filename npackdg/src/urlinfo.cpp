#include "urlinfo.h"

URLInfo::URLInfo(): QObject(nullptr),
        address(""), size(-1), sizeModified(0)
{

}

URLInfo::URLInfo(const QString &address) : QObject(nullptr),
        address(address), size(-1), sizeModified(0)
{

}

URLInfo::URLInfo(const URLInfo& v) : QObject(nullptr),
        address(v.address), size(v.size), sizeModified(v.sizeModified)
{

}

URLInfo &URLInfo::operator=(const URLInfo &v)
{
    this->address = v.address;
    this->size = v.size;
    this->sizeModified = v.sizeModified;
    return *this;
}
