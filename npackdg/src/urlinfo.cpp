#include "urlinfo.h"

URLInfo::URLInfo(): QObject(0),
        address(""), size(-1), sizeModified(0)
{

}

URLInfo::URLInfo(const QString &address) : QObject(0),
        address(address), size(-1), sizeModified(0)
{

}

URLInfo::URLInfo(const URLInfo& v) : QObject(0),
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
