#include <QUrl>
#include <QJsonArray>

#include "package.h"
#include "wpmutils.h"

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
    if (changelog.isEmpty())
        links.remove("icon");
    else
        links.replace("icon", changelog);
}

void Package::setIcon(const QString &icon)
{
    if (icon.isEmpty())
        links.remove("icon");
    else
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
    QList<QString> rels = links.uniqueKeys();
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

void Package::toJSON(QJsonObject& w) const
{
    w["name"] = this->name;
    w["title"] = this->title;
    if (!this->url.isEmpty())
        w["url"] = this->url;
    if (!this->description.isEmpty())
        w["description"] = this->description;
    if (!this->getIcon().isEmpty())
        w["icon"] = this->getIcon();
    if (!this->license.isEmpty())
        w["license"] = this->license;

    if (!this->categories.isEmpty()) {
        QJsonArray category;
        for (int i = 0; i < this->categories.count(); i++) {
            category.append(this->categories.at(i));
        }
        w["categories"] = category;
    }

    QJsonArray link;
    QList<QString> rels = links.uniqueKeys();
    for (int i = 0; i < rels.size(); i++) {
        QString rel = rels.at(i);
        QList<QString> hrefs = links.values(rel);
        for (int j = hrefs.size() - 1; j >= 0; j--) {
            QJsonObject obj;
            obj["rel"] = rel;
            obj["href"] = hrefs.at(j);
            link.append(obj);
        }
    }
    if (!link.isEmpty())
        w["links"] = link;
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
