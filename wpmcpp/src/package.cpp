#include <QUrl>

#include "package.h"
#include "xmlutils.h"

int Package::indexOf(const QList<Package*> pvs, Package* f)
{
    int r = -1;
    for (int i = 0; i < pvs.count(); i++) {
        Package* pv = pvs.at(i);
        if (pv->name == f->name) {
            r = i;
            break;
        }
    }
    return r;
}

Package *Package::parse(QDomElement *e, QString *err, bool validate)
{
    // TODO: validation
    Package* p = new Package("test", "test");
    p->name = e->attribute("name");
    p->title = XMLUtils::getTagContent(*e, "title");
    p->url = XMLUtils::getTagContent(*e, "url");
    p->description = XMLUtils::getTagContent(*e, "description");
    p->license = XMLUtils::getTagContent(*e, "license");

    QDomNodeList list = e->elementsByTagName("category");
    for (int i = 0; i < list.count(); i++) {
        QDomElement ch = list.at(i).toElement();
        QDomNode txt = ch.firstChild();
        if (txt.isText()) {
            p->categories.append(txt.nodeValue().trimmed());
        }
    }

    list = e->elementsByTagName("link");
    for (int i = 0; i < list.count(); i++) {
        QDomElement ch = list.at(i).toElement();
        QString rel = ch.attribute("rel");
        QString href = ch.attribute("href");
        p->links.insert(rel, href);
    }

    if (p->getIcon().isEmpty()) {
        p->setIcon(XMLUtils::getTagContent(*e, "icon"));
    }

    return p;
}

bool Package::contains(const QList<Package*> &list, Package *pv)
{
    return indexOf(list, pv) >= 0;
}

Package::Package(const QString& name, const QString& title)
{
    this->name = name;
    this->title = title;
}

QString Package::getIcon() const
{
    QString r;
    QList<QString> values = links.values("icon");
    if (!values.isEmpty())
        r = values.last();
    return r;
}

QString Package::getChangeLog() const
{
    QString r;
    QList<QString> values = links.values("changelog");
    if (!values.isEmpty())
        r = values.last();
    return r;
}

void Package::setChangeLog(const QString &changelog)
{
    links.replace("icon", changelog);
}

void Package::setIcon(const QString &icon)
{
    links.replace("icon", icon);
}

QString Package::getShortName() const
{
    QString r;
    int index = this->name.lastIndexOf('.');
    if (index < 0)
        r = this->name;
    else
        r = this->name.mid(index + 1);
    return r;
}

bool Package::isValidName(const QString& name)
{
    bool r = false;
    if (!name.isEmpty() && !name.contains(" ") && !name.contains("..")) {
        r = true;
    }
    return r;
}

bool Package::isValidURL(const QString& url)
{
    bool r = true;
    if (url.trimmed().isEmpty())
        r = false;
    else {
        QUrl u(url);
        r = u.isValid() && !u.isRelative() &&
                (u.scheme() == "http" || u.scheme() == "https" ||
                u.scheme() == "file");
    }
    return r;
}

void Package::toXML(QXmlStreamWriter *w) const
{
    w->writeStartElement("package");
    w->writeAttribute("name", this->name);
    w->writeTextElement("title", this->title);
    if (!this->url.isEmpty())
        w->writeTextElement("url", this->url);
    if (!this->description.isEmpty())
        w->writeTextElement("description", this->description);
    if (!this->getIcon().isEmpty())
        w->writeTextElement("icon", this->getIcon());
    if (!this->license.isEmpty())
        w->writeTextElement("license", this->license);
    for (int i = 0; i < this->categories.count(); i++) {
        w->writeTextElement("category", this->categories.at(i));
    }

    // <link>
    QList<QString> rels = links.keys();
    for (int i = 0; i < rels.size(); i++) {
        QString rel = rels.at(i);
        QList<QString> hrefs = links.values(rel);
        for (int j = hrefs.size() - 1; j >= 0; j--) {
            w->writeStartElement("link");
            w->writeAttribute("rel", rel);
            w->writeAttribute("href", hrefs.at(j));
            w->writeEndElement();
        }
    }

    w->writeEndElement();
}

void Package::saveTo(QDomElement& e) const {
    e.setAttribute("name", name);
    XMLUtils::addTextTag(e, "title", title);
    if (!this->url.isEmpty())
        XMLUtils::addTextTag(e, "url", this->url);
    if (!description.isEmpty())
        XMLUtils::addTextTag(e, "description", description);
    if (!this->getIcon().isEmpty())
        XMLUtils::addTextTag(e, "icon", this->getIcon());
    if (!this->license.isEmpty())
        XMLUtils::addTextTag(e, "license", this->license);
    for (int i = 0; i < this->categories.count(); i++) {
        XMLUtils::addTextTag(e, "category", this->categories.at(i));
    }

    // <link>
    QList<QString> rels = links.keys();
    for (int i = 0; i < rels.size(); i++) {
        QString rel = rels.at(i);
        QList<QString> hrefs = links.values(rel);
        for (int j = hrefs.size() - 1; j >= 0; j--) {
            QDomElement e = e.ownerDocument().createElement("link");
            e.setAttribute("rel", rel);
            e.setAttribute("href", hrefs.at(j));
            e.appendChild(e);
        }
    }
}

Package *Package::clone() const
{
    Package* np = new Package(this->name, this->title);
    np->url = this->url;
    np->description = this->description;
    np->license = this->license;
    np->categories = this->categories;
    np->links = this->links;

    return np;
}
