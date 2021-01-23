#ifndef REPOSITORIESITEMMODEL_H
#define REPOSITORIESITEMMODEL_H

#include <QAbstractTableModel>
#include <QCache>
#include <QBrush>

#include "package.h"
#include "version.h"

/**
 * @brief list of repositories for the settings tab
 */
class RepositoriesItemModel: public QAbstractTableModel
{
public:
    /**
     * @brief one repository
     */
    class Entry {
    public:
        /** true = the repository is enabled */
        bool enabled;

        /** URL */
        QString url;

        /** comment */
        QString comment;

        Entry();
    };
private:
    std::vector<Entry*> entries;
public:
    /**
     * @brief -
     */
    RepositoriesItemModel();

    virtual ~RepositoriesItemModel();

    int rowCount(const QModelIndex &parent) const;

    int columnCount(const QModelIndex &parent) const;

    QVariant data(const QModelIndex &index, int role) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                          const QModelIndex &destinationParent, int destinationChild);

    /**
     * @brief changes the list of URLs
     * @param urls [move] list of URLs
     */
    void setURLs(const std::vector<Entry *> &entries);

    /**
     * @brief adds a new repository
     */
    void add();

    /**
     * @brief removes a row
     * @param row row index
     */
    void remove(int row);

    /**
     * @brief returns the list of repository entries
     * @return the entries
     */
    std::vector<Entry*> getEntries();
};

#endif // REPOSITORIESITEMMODEL_H
