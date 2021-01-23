#include <cmath>

#include <QApplication>

#include "dbrepository.h"
#include "license.h"
#include "packageitemmodel.h"
#include "abstractrepository.h"
#include "mainwindow.h"
#include "wpmutils.h"

PackageItemModel::PackageItemModel(const std::vector<QString>& packages) :
        obsoleteBrush(QColor(255, 0xc7, 0xc7)),
        maxStars(-1)
{
    this->packages = packages;
}

PackageItemModel::~PackageItemModel()
{
}

int PackageItemModel::rowCount(const QModelIndex &/*parent*/) const
{
    return this->packages.size();
}

int PackageItemModel::columnCount(const QModelIndex &/*parent*/) const
{
    return 10;
}

PackageItemModel::Info* PackageItemModel::createInfo(
        Package* p) const
{
    Info* r = new Info();

    DBRepository* rep = DBRepository::getDefault();

    // error is ignored here
    QString err;
    std::vector<PackageVersion*> pvs = rep->getPackageVersions_(p->name, &err);

    PackageVersion* newestInstallable = nullptr;
    PackageVersion* newestInstalled = nullptr;
    for (auto pv: pvs) {
        if (pv->installed()) {
            if (!r->installed.isEmpty())
                r->installed.append(", ");
            r->installed.append(pv->version.getVersionString());
            if (!newestInstalled ||
                    newestInstalled->version.compare(pv->version) < 0)
                newestInstalled = pv;
        }

        if (pv->download.isValid()) {
            if (!newestInstallable ||
                    newestInstallable->version.compare(pv->version) < 0)
                newestInstallable = pv;
        }
    }

    if (newestInstallable) {
        r->avail = newestInstallable->version.getVersionString();
        r->newestDownloadURL = newestInstallable->download.toString(
                QUrl::FullyEncoded);
    }

    r->up2date = !(newestInstalled && newestInstallable &&
            newestInstallable->version.compare(
            newestInstalled->version) > 0);

    qDeleteAll(pvs);
    pvs.clear();

    QString s = p->description;
    if (s.length() > 200) {
        s = s.left(200) + "...";
    }
    r->shortenDescription = s;

    r->title = p->title;

    // the error message is ignored
    License* lic = rep->findLicense_(p->license, &err);
    if (lic) {
        r->licenseTitle = lic->title;
        delete lic;
    }

    r->icon = p->getIcon();

    if (p->categories.size() > 0) {
        r->category = p->categories.at(0);
    } else {
        r->category.clear();
    }

    r->tags = WPMUtils::join(p->tags, QStringLiteral(", "));

    r->stars = p->stars;

    return r;
}

QVariant PackageItemModel::data(const QModelIndex &index, int role) const
{
    QString p = this->packages.at(index.row());

    // performance optimization
    if (role == Qt::UserRole && index.column() != 0) {
        return p;
    }

    QVariant r;
    DBRepository* rep = DBRepository::getDefault();
    Info* cached = this->cache.object(p);
    bool insertIntoCache = false;
    if (!cached) {
        Package* pk = rep->findPackage_(p);
        cached = createInfo(pk);
        delete pk;
        insertIntoCache = true;
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 1:
                r = cached->title;
                break;
            case 2: {
                r = cached->shortenDescription;
                break;
            }
            case 3: {
                r = cached->avail;
                break;
            }
            case 4: {
                r = cached->installed;
                break;
            }
            case 5: {
                r = cached->licenseTitle;
                break;
            }
            case 6: {
                MainWindow* mw = MainWindow::getInstance();
                QString v;
                if (cached->newestDownloadURL.isEmpty()) {
                    v = "";
                } else {
                    int64_t sz = mw->fileLoader.downloadSizeOrQueue(
                            cached->newestDownloadURL);
                    if (sz >= 0)
                        v = QString::number(
                                    static_cast<double>(sz) /
                                    (1024.0 * 1024.0), 'f', 1) +
                            " MiB";
                    else if (sz == -1)
                        v = QObject::tr("computing");
                    else
                        v = "";
                }

                r = QVariant::fromValue(v);
                break;
            }
            case 7: {
                r = cached->category;
                break;
            }
            case 8: {
                r = cached->tags;
                break;
            }
            case 9: {
                if (cached->stars > 0)
                    r = QString::number(cached->stars);
                break;
            }
        }
    } else if (role == Qt::UserRole) {
        switch (index.column()) {
            case 0:
                r = QVariant::fromValue(cached->icon);
                break;
            default:
                r = p;
        }
    } else if (role == Qt::DecorationRole) {
        switch (index.column()) {
            case 0: {
                MainWindow* mw = MainWindow::getInstance();
                if (!cached->icon.isEmpty()) {
                    r = QVariant::fromValue(mw->downloadIcon(cached->icon));
                } else {
                    r = QVariant::fromValue(MainWindow::genericAppIcon);
                }
                break;
            }
        }
    } else if (role == Qt::BackgroundRole) {
        switch (index.column()) {
            case 4: {
                if (!cached->up2date)
                    r = QVariant::fromValue(obsoleteBrush);
                break;
            }
            case 9: {
                if (cached->stars > 0) {
                    if (maxStars < 0) {
                        QString err;
                        maxStars = rep->getMaxStars(&err);
                    }

                    int f = static_cast<int>(100.0 *
                        (1.0 + log(maxStars / cached->stars)));
                    r = QVariant::fromValue(QBrush(QColor(
                            0xd4, 0xed, 0xda).lighter(f)));
                }
                break;
            }
        }
    } else if (role == Qt::StatusTipRole) {
        switch (index.column()) {
            case 1:
                r = p;
                break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case 6:
            case 9:
                r = Qt::AlignRight + Qt::AlignVCenter;
                break;
            default:
                r = Qt::AlignLeft + Qt::AlignVCenter;
        }
    }

    if (insertIntoCache)
        this->cache.insert(p, cached);

    return r;
}

QVariant PackageItemModel::headerData(int section, Qt::Orientation orientation,
        int role) const
{
    QVariant r;
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
                case 0:
                    r = QObject::tr("Icon");
                    break;
                case 1:
                    r = QObject::tr("Title");
                    break;
                case 2:
                    r = QObject::tr("Description");
                    break;
                case 3:
                    r = QObject::tr("Available");
                    break;
                case 4:
                    r = QObject::tr("Installed");
                    break;
                case 5:
                    r = QObject::tr("License");
                    break;
                case 6:
                    r = QObject::tr("Download size");
                    break;
                case 7:
                    r = QObject::tr("Category");
                    break;
                case 8:
                    r = QObject::tr("Tags");
                    break;
                case 9:
                    r = QObject::tr("Stars");
                    break;
            }
        } else {
            r = QString("%1").arg(section + 1);
        }
    }
    return r;
}

void PackageItemModel::setPackages(const std::vector<QString>& packages)
{
    this->beginResetModel();
    this->packages = packages;
    this->endResetModel();
}

void PackageItemModel::iconUpdated(const QString &/*url*/)
{
    this->dataChanged(this->index(0, 0), this->index(
            this->packages.size() - 1, 0));
}

void PackageItemModel::downloadSizeUpdated(const QString &/*url*/)
{
    this->dataChanged(this->index(0, 5), this->index(
            this->packages.size() - 1, 5));
}

void PackageItemModel::installedStatusChanged(const QString& package,
        const Version& /*version*/)
{
    //qCDebug(npackd) << "PackageItemModel::installedStatusChanged" << package <<
    //        version.getVersionString();
    this->cache.remove(package);
    for (int i = 0; i < static_cast<int>(this->packages.size()); i++) {
        QString p = this->packages.at(i);
        if (p == package) {
            this->dataChanged(this->index(i, 4), this->index(i, 4));
        }
    }
}

void PackageItemModel::clearCache()
{
    this->cache.clear();
    this->dataChanged(this->index(0, 3),
            this->index(this->packages.size() - 1, 4));
}
