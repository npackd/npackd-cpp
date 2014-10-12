#include "fileloaderitem.h"

FileLoaderItem::FileLoaderItem() : contentRequired(true), size(-1L)
{
}

FileLoaderItem::~FileLoaderItem()
{
}

FileLoaderItem::FileLoaderItem(const FileLoaderItem& it) : QObject()
{
    this->contentRequired = it.contentRequired;
    this->size = it.size;
    this->url = it.url;
    this->f = it.f;
}

FileLoaderItem& FileLoaderItem::operator=(const FileLoaderItem& it)
{
    this->contentRequired = it.contentRequired;
    this->size = it.size;
    this->url = it.url;
    this->f = it.f;
    return *this;
}

bool FileLoaderItem::operator==(const FileLoaderItem &it)
{
    return this->url == it.url;
}
