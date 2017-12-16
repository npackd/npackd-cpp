#include "mysqlquery.h"
#include "wpmutils.h"

#if defined(NDEBUG) // this is a standard C++ macro
bool MySQLQuery::debug = false;
#else
bool MySQLQuery::debug = true;
#endif

MySQLQuery::MySQLQuery(QSqlDatabase db) : QSqlQuery(db)
{
}

bool MySQLQuery::exec(const QString &query)
{
    if (debug)
        WPMUtils::writeln(query);

    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec(query);
    // qCDebug(npackd) << query << (GetTickCount() - start);
    return r;
}

bool MySQLQuery::exec()
{
    if (debug)
        WPMUtils::writeln(this->lastQuery());

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

