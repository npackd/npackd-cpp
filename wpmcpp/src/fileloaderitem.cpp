#include "fileloaderitem.h"

FileLoaderItem::FileLoaderItem()
{
}

FileLoaderItem::~FileLoaderItem()
{
}

FileLoaderItem::FileLoaderItem(const FileLoaderItem& it) : QObject()
{
    this->url = it.url;
    this->f = it.f;
}

FileLoaderItem& FileLoaderItem::operator=(const FileLoaderItem& it)
{
    this->url = it.url;
    this->f = it.f;
    return *this;
}
