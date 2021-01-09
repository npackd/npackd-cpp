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

std::vector<QString> Package::getLinks(const QString &rel) const
{
    auto it = links.equal_range(rel);
    std::vector<QString> r;
    for (auto it2 = it.first; it2 != it.second; ++it2) {
        r.push_back(it2->second);
    }
    return r;
}

QString Package::getIcon() const
{
    QString r;
    std::vector<QString> values = getLinks("icon");
    if (values.size() > 0)
        r = values.back();
    return r;
}

QString Package::getChangeLog() const
{
    QString r;
    std::vector<QString> values = getLinks("changelog");
    if (values.size() > 0)
        r = values.back();
    return r;
}

void Package::setChangeLog(const QString &changelog)
{
    links.erase("changelog");
    if (!changelog.isEmpty())
        links.insert({"changelog", changelog});
}

QString Package::getIssueTracker() const
{
    QString r;
    std::vector<QString> values = getLinks("issues");
    if (values.size() > 0)
        r = values.back();
    return r;
}

void Package::setIssueTracker(const QString &v)
{
    links.erase("issues");
    if (!v.isEmpty())
        links.insert({"issues", v});
}

void Package::setIcon(const QString &icon)
{
    links.erase("icon");
    if (!icon.isEmpty())
        links.insert({"icon", icon});
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
    for (auto&& it: links) {
        w->writeStartElement("link");
        w->writeAttribute("rel", it.first);
        w->writeAttribute("href", it.second);
        w->writeEndElement();
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
    for (auto&& it: links) {
        QJsonObject obj;
        obj["rel"] = it.first;
        obj["href"] = it.second;
        link.append(obj);
    }
    if (!link.isEmpty())
        w["links"] = link;

    QJsonArray installed;
    std::vector<InstalledPackageVersion*> ipvs = InstalledPackages::getDefault()->
            getByPackage(this->name);
    for (auto ipv: ipvs) {
        QJsonObject obj;
        obj["version"] = ipv->version.getVersionString();
        obj["where"] = ipv->directory;
        installed.append(obj);
    }
    if (!installed.isEmpty())
        w["installed"] = installed;
}

