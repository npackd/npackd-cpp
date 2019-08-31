#include "QLoggingCategory"

#include "mysqlquery.h"
#include "wpmutils.h"

MySQLQuery::MySQLQuery(QSqlDatabase db) : QSqlQuery(db)
{
}

bool MySQLQuery::exec(const QString &query)
{
    qCDebug(npackd) << query;

    bool e = npackd().isDebugEnabled();

    DWORD start = 0;
    if (e)
        start = GetTickCount();

    bool r = QSqlQuery::exec(query);

    if (e) {
        qCDebug(npackd) << (GetTickCount() - start) << "ms";
    }

    return r;
}

bool MySQLQuery::exec()
{
    qCDebug(npackd) << this->lastQuery();

    bool e = npackd().isDebugEnabled();

    DWORD start = 0;
    if (e)
        start = GetTickCount();

    bool r = QSqlQuery::exec();

    if (e) {
        qCDebug(npackd) << (GetTickCount() - start) << "ms";
    }

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

