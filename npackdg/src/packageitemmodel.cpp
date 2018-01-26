#include <stdint.h>

#include <QSharedPointer>
#include <QApplication>

#include "dbrepository.h"
#include "license.h"
#include "packageitemmodel.h"
#include "abstractrepository.h"
#include "mainwindow.h"
#include "wpmutils.h"

PackageItemModel::PackageItemModel(const QStringList& packages) :
        obsoleteBrush(QColor(255, 0xc7, 0xc7))
{
    this->packages = packages;
}

PackageItemModel::~PackageItemModel()
{
}

int PackageItemModel::rowCount(const QModelIndex &parent) const
{
    return this->packages.count();
}

int PackageItemModel::columnCount(const QModelIndex &parent) const
{
    return 7;
}

PackageItemModel::Info* PackageItemModel::createInfo(
        Package* p) const
{
    Info* r = new Info();

    DBRepository* rep = DBRepository::getDefault();

    // error is ignored here
    QString err;
    QList<PackageVersion*> pvs = rep->getPackageVersions_(p->name, &err);

    PackageVersion* newestInstallable = 0;
    PackageVersion* newestInstalled = 0;
    for (int j = 0; j < pvs.count(); j++) {
        PackageVersion* pv = pvs.at(j);
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
    QSharedPointer<License> lic(rep->findLicense_(
            p->license, &err));
    if (lic)
        r->licenseTitle = lic->title;

    r->icon = p->getIcon();

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
                QString err;
                QString v;
                if (cached->newestDownloadURL.isEmpty()) {
                    v = "";
                } else {
                    int64_t sz = mw->downloadSizeFinder.downloadOrQueue(
                            cached->newestDownloadURL);
                    if (sz >= 0)
                        v = QString::number(
                            ((double) sz) / (1024.0 * 1024.0), 'f', 1) +
                            " MiB";
                    else if (sz == -1)
                        v = QObject::tr("computing");
                    else
                        v = "";
                }

                r = qVariantFromValue(v);
                break;
            }
        }
    } else if (role == Qt::UserRole) {
        switch (index.column()) {
            case 0:
                r = qVariantFromValue(cached->icon);
                break;
            default:
                r = p;
        }
    } else if (role == Qt::DecorationRole) {
        switch (index.column()) {
            case 0: {
                MainWindow* mw = MainWindow::getInstance();
                if (!cached->icon.isEmpty()) {
                    r = qVariantFromValue(mw->downloadIcon(cached->icon));
                } else {
                    r = qVariantFromValue(MainWindow::genericAppIcon);
                }
                break;
            }
        }
    } else if (role == Qt::BackgroundRole) {
        switch (index.column()) {
            case 4: {
                if (!cached->up2date)
                    r = qVariantFromValue(obsoleteBrush);
                break;
            }
        }
    } else if (role == Qt::StatusTipRole) {
        switch (index.column()) {
            case 1:
                r = p;
                break;
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
            }
        } else {
            r = QString("%1").arg(section + 1);
        }
    }
    return r;
}

void PackageItemModel::setPackages(const QStringList& packages)
{
    this->beginResetModel();
    this->packages = packages;
    this->endResetModel();
}

void PackageItemModel::iconUpdated(const QString &url)
{
    this->dataChanged(this->index(0, 0), this->index(
            this->packages.count() - 1, 0));
}

void PackageItemModel::downloadSizeUpdated(const QString &url)
{
    this->dataChanged(this->index(0, 5), this->index(
            this->packages.count() - 1, 5));
}

void PackageItemModel::installedStatusChanged(const QString& package,
        const Version& version)
{
    //qCDebug(npackd) << "PackageItemModel::installedStatusChanged" << package <<
    //        version.getVersionString();
    this->cache.remove(package);
    for (int i = 0; i < this->packages.count(); i++) {
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
            this->index(this->packages.count() - 1, 4));
}
