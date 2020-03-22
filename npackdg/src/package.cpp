#include <QUrl>
#include <QJsonArray>

#include "package.h"
#include "wpmutils.h"
#include "installedpackages.h"

Package::Package(const QString& name, const QString& title): stars(0)
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
        links.remove("changelog");
    else
        links.replace("changelog", changelog);
}

QString Package::getIssueTracker() const
{
    QString r;
    QList<QString> values = links.values("issues");
    if (!values.isEmpty())
        r = values.last();
    return r;
}

void Package::setIssueTracker(const QString &v)
{
    if (v.isEmpty())
        links.remove("issues");
    else
        links.replace("issues", v);
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
    return getShortName(this->name);
}

QString Package::getShortName(const QString& fullname)
{
    QString r;
    int index = fullname.lastIndexOf('.');
    if (index < 0)
        r = fullname;
    else
        r = fullname.mid(index + 1);
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

    for (int i = 0; i < this->tags.count(); i++) {
        w->writeTextElement("tag", this->tags.at(i));
    }

    if (this->stars > 0) {
        w->writeTextElement("tag", QString::number(this->stars));
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

    if (!this->tags.isEmpty()) {
        QJsonArray tag;
        for (int i = 0; i < this->tags.count(); i++) {
            tag.append(this->tags.at(i));
        }
        w["tags"] = tag;
    }

    if (this->stars > 0) {
        w["stars"] = QString::number(this->stars);
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

    QJsonArray installed;
    QList<InstalledPackageVersion*> ipvs = InstalledPackages::getDefault()->
            getByPackage(this->name);
    for (int i = 0; i < ipvs.size(); i++) {
        InstalledPackageVersion* ipv = ipvs.at(i);
        QJsonObject obj;
        obj["version"] = ipv->version.getVersionString();
        obj["where"] = ipv->directory;
        installed.append(obj);
    }
    if (!installed.isEmpty())
        w["installed"] = installed;
}

