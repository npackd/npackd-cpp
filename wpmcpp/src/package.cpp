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

void Package::setIcon(const QString &icon)
{
    links.remove("icon");
    links.insert("icon", icon);
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
    if (!this->changelog.isEmpty())
        XMLUtils::addTextTag(e, "changelog", this->changelog);
    for (int i = 0; i < this->screenshots.count(); i++) {
        XMLUtils::addTextTag(e, "screenshot", this->screenshots.at(i));
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
    np->changelog = this->changelog;
    np->screenshots = this->screenshots;
    np->links = this->links;

    return np;
}
