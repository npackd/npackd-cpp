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

void License::toXML(QXmlStreamWriter &w) const
{
    w.writeStartElement("license");
    w.writeAttribute("name", this->name);
    if (!title.isEmpty())
        w.writeTextElement("title", this->title);
    if (!this->url.isEmpty())
        w.writeTextElement("url", this->url);
    w.writeEndElement();
}
