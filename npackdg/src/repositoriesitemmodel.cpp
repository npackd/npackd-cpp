#include "repositoriesitemmodel.h"

RepositoriesItemModel::RepositoriesItemModel()
{

}

RepositoriesItemModel::~RepositoriesItemModel()
{
    qDeleteAll(entries);
}

int RepositoriesItemModel::rowCount(const QModelIndex &/*parent*/) const
{
    return entries.size();
}

int RepositoriesItemModel::columnCount(const QModelIndex &/*parent*/) const
{
    return 3;
}

QVariant RepositoriesItemModel::data(const QModelIndex &index, int role) const
{
    Entry* e = this->entries.at(index.row());

    QVariant r;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 1: {
                r = e->url;
                break;
            }
            case 2: {
                r = e->comment;
                break;
            }
        }
    } else if (role == Qt::EditRole) {
        switch (index.column()) {
            case 1: {
                r = e->url;
                break;
            }
            case 2: {
                r = e->comment;
                break;
            }
        }
    } else if (role == Qt::CheckStateRole) {
        switch (index.column()) {
            case 0:
                r = e->enabled ? Qt::Checked : Qt::Unchecked;
                break;
        }
    }

    return r;
}

Qt::ItemFlags RepositoriesItemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    switch (index.column()) {
        case 0: {
            f |= Qt::ItemIsUserCheckable;
            break;
        }
        case 1: {
            f |= Qt::ItemIsEditable;
            break;
        }
        case 2: {
            f |= Qt::ItemIsEditable;
            break;
        }
    }

    return f;
}

QVariant RepositoriesItemModel::headerData(int section, Qt::Orientation orientation,
        int role) const
{
    QVariant r;
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            switch (section) {
                case 0:
                    r = QObject::tr("Enabled");
                    break;
                case 1:
                    r = QObject::tr("URL");
                    break;
                case 2:
                    r = QObject::tr("Comment");
                    break;
            }
        } else {
            r = QString("%1").arg(section + 1);
        }
    }
    return r;
}

bool RepositoriesItemModel::setData(const QModelIndex &index,
                              const QVariant &value, int role)
{
    bool r = false;

    if (index.isValid()) {
        Entry* e = this->entries.at(index.row());
        if (role == Qt::EditRole) {
            switch (index.column()) {
                case 1: {
                    e->url = value.toString();
                    r = true;
                    break;
                }
                case 2: {
                    e->comment = value.toString();
                    r = true;
                    break;
                }
            }
        } else if (role == Qt::CheckStateRole) {
            switch (index.column()) {
                case 0:
                    e->enabled = value.toBool();
                    r = true;
                    break;
            }
        }
    }

    if (r)
        emit dataChanged(index, index);

    return r;
}

bool RepositoriesItemModel::moveRows(const QModelIndex &sourceParent, int sourceRow,
        int count, const QModelIndex &destinationParent, int destinationChild)
{
    beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild);
    auto v = this->entries.at(sourceRow);
    this->entries.erase(this->entries.begin() + sourceRow);
    this->entries.insert(this->entries.begin() + destinationChild, v);
    endMoveRows();
    return true;
}

void RepositoriesItemModel::setURLs(const std::vector<Entry*> &entries)
{
    this->beginResetModel();
    qDeleteAll(this->entries);
    this->entries.clear();
    this->entries = entries;
    this->endResetModel();
}

void RepositoriesItemModel::add()
{
    this->beginInsertRows(QModelIndex(), 0, 0);
    Entry* e = new Entry();
    e->enabled = true;
    this->entries.insert(this->entries.begin(), e);
    this->endInsertRows();
}

void RepositoriesItemModel::remove(int row)
{
    this->beginRemoveRows(QModelIndex(), row, row);
    Entry* e = this->entries.at(row);
    delete e;
    this->entries.erase(this->entries.begin() + row);
    this->endRemoveRows();
}

std::vector<RepositoriesItemModel::Entry *> RepositoriesItemModel::getEntries()
{
    return entries;
}

RepositoriesItemModel::Entry::Entry(): enabled(false)
{

}
