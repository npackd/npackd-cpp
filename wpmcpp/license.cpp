#include "license.h"

License::License(QString name, QString title)
{
    this->name = name;
    this->title = title;
}

License *License::clone() const
{
    License* r = new License(this->name, this->title);
    r->description = this->description;
    r->url = this->url;
    return r;
}
