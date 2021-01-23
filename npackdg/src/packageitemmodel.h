#ifndef PACKAGEITEMMODEL_H
#define PACKAGEITEMMODEL_H

#include <QAbstractTableModel>
#include <QCache>
#include <QBrush>

#include "package.h"
#include "version.h"

/**
 * @brief shows packages
 */
class PackageItemModel: public QAbstractTableModel
{
    QBrush obsoleteBrush;

    mutable int maxStars;

    std::vector<QString> packages;

    /**
     * @brief package information for one row
     */
    class Info {
    public:
        QString avail;
        QString installed;
        bool up2date;
        QString newestDownloadURL;
        QString shortenDescription;
        QString title;
        QString licenseTitle;
        QString icon;
        QString category;
        QString tags;
        int stars;
    };

    mutable QCache<QString, Info> cache;

    Info *createInfo(Package *p) const;
public:
    /**
     * @param packages list of package names
     */
    PackageItemModel(const std::vector<QString> &packages);

    ~PackageItemModel();

    int rowCount(const QModelIndex &parent) const;

    int columnCount(const QModelIndex &parent) const;

    QVariant data(const QModelIndex &index, int role) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    /**
     * @brief changes the list of packages
     * @param packages list of package names
     */
    void setPackages(const std::vector<QString> &packages);

    /**
     * @brief should be called if an icon has changed
     * @param url URL of the icon
     */
    void iconUpdated(const QString& url);

    /**
     * @brief should be called if the "installed" status of a package version
     *     has changed
     * @param package full package name
     * @param version version number
     */
    void installedStatusChanged(const QString &package, const Version &version);

    /**
     * @brief clears the cache
     */
    void clearCache();

    /**
     * @brief should be called if a download size for a package was computed
     * @param url URL of the binary
     */
    void downloadSizeUpdated(const QString &url);
};

#endif // PACKAGEITEMMODEL_H
