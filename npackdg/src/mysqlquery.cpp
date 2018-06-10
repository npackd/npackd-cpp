#include "QLoggingCategory"

#include "mysqlquery.h"
#include "wpmutils.h"

MySQLQuery::MySQLQuery(QSqlDatabase db) : QSqlQuery(db)
{
}

bool MySQLQuery::exec(const QString &query)
{
    qCDebug(npackd) << query;

    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec(query);
    // qCDebug(npackd) << query << (GetTickCount() - start);
    return r;
}

bool MySQLQuery::exec()
{
    qCDebug(npackd) << this->lastQuery();

    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec();
    // qCDebug(npackd) << this->lastQuery() << (GetTickCount() - start);
    return r;
}

bool MySQLQuery::prepare(const QString& query)
{
    return QSqlQuery::prepare(query);
}

bool MySQLQuery::next()
{
    return QSqlQuery::next();
}

