#include <limits>

#include "wpmutils.h"
#include "dependency.h"
#include "packageversion.h"
#include "package.h"
#include "installedpackages.h"
#include "installedpackageversion.h"

Dependency::Dependency()
{
    this->minIncluded = true;
    this->min.setVersion(0, 0);
    this->maxIncluded = false;
    this->max.setVersion(1, 0);
}

QString Dependency::versionsToString() const
{
    QString res;
    if (minIncluded)
        res.append('[');
    else
        res.append('(');

    res.append(this->min.getVersionString());

    res.append(", ");

    res.append(this->max.getVersionString());

    if (maxIncluded)
        res.append(']');
    else
        res.append(')');
    return res;
}

Dependency *Dependency::clone() const
{
    Dependency* r = new Dependency();
    r->package = this->package;
    r->min = this->min;
    r->max = this->max;
    r->minIncluded = this->minIncluded;
    r->maxIncluded = this->maxIncluded;
    r->var = this->var;
    return r;
}

bool Dependency::autoFulfilledIf(const Dependency& dep)
{
    bool r;
    if (this->package == dep.package) {
        int left = this->min.compare(dep.min);
        int right = this->max.compare(dep.max);

        bool leftOK;
        if (left < 0)
            leftOK = true;
        else if (left == 0)
            leftOK = this->minIncluded || !dep.minIncluded;
        else
            leftOK = false;
        bool rightOK;
        if (right > 0)
            rightOK = true;
        else if (right == 0)
            rightOK = this->maxIncluded || !dep.maxIncluded;
        else
            rightOK = false;

        r = leftOK && rightOK;
    } else {
        r = false;
    }
    return r;
}

bool Dependency::setVersions(const QString &versions)
{
    QString versions_ = versions;

    bool minIncluded_, maxIncluded_;

    // qCDebug(npackd) << "Repository::createDependency.1" << versions;

    if (versions_.startsWith('['))
        minIncluded_ = true;
    else if (versions_.startsWith('('))
        minIncluded_ = false;
    else
        return false;
    versions_.remove(0, 1);

    // qCDebug(npackd) << "Repository::createDependency.1.1" << versions;

    if (versions_.endsWith(']'))
        maxIncluded_ = true;
    else if (versions_.endsWith(')'))
        maxIncluded_ = false;
    else
        return false;
    versions_.chop(1);

    // qCDebug(npackd) << "Repository::createDependency.2";

    std::vector<QString> parts = WPMUtils::split(versions_, ',');
    if (parts.size() != 2)
        return false;

    Version min_, max_;
    if (!min_.setVersion(parts.at(0).trimmed()) ||
            !max_.setVersion(parts.at(1).trimmed()))
        return false;
    this->minIncluded = minIncluded_;
    this->min = min_;
    this->maxIncluded = maxIncluded_;
    this->max = max_;

    return true;
}

void Dependency::setUnboundedVersions()
{
    this->minIncluded = true;
    this->min.setVersion(0, 0);
    this->maxIncluded = true;
    this->max.setVersion(std::numeric_limits<int>::max(), 0);
}

void Dependency::setExactVersion(const Version &version)
{
    this->minIncluded = true;
    this->min = version;
    this->maxIncluded = true;
    this->max = version;
}

bool Dependency::test(const Version& v) const
{
    int a = v.compare(this->min);
    int b = v.compare(this->max);

    bool low;
    if (minIncluded)
        low = a >= 0;
    else
        low = a > 0;

    bool high;
    if (maxIncluded)
        high = b <= 0;
    else
        high = b < 0;

    return low && high;
}

