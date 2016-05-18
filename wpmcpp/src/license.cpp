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

/*
License* Repository::createLicense(QDomElement* e, QString* error)
{
    License* a = 0;
    *error = "";

    QString name = e->attribute("name");
    *error = WPMUtils::validateFullPackageName(name);
    if (!error->isEmpty()) {
        error->prepend(QObject::tr("Error in attribute 'name' in <package>: "));
    }

    if (error->isEmpty()) {
        a = new License(name, name);
        QDomNodeList nl = e->elementsByTagName("title");
        if (nl.count() != 0)
            a->title = nl.at(0).firstChild().nodeValue();
        nl = e->elementsByTagName("url");
        if (nl.count() != 0)
            a->url = nl.at(0).firstChild().nodeValue();
        nl = e->elementsByTagName("description");
        if (nl.count() != 0)
            a->description = nl.at(0).firstChild().nodeValue();
    }

    if (!error->isEmpty()) {
        delete a;
        a = 0;
    }

    return a;
}
 */
