#include "mysqlquery.h"
#include "wpmutils.h"

bool MySQLQuery::debug = false;

MySQLQuery::MySQLQuery(QSqlDatabase db) : QSqlQuery(db)
{
}

bool MySQLQuery::exec(const QString &query)
{
    if (debug)
        WPMUtils::writeln(query);

    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec(query);
    // qDebug() << query << (GetTickCount() - start);
    return r;
}

bool MySQLQuery::exec()
{
    if (debug)
        WPMUtils::writeln(this->lastQuery());

    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec();
    // qDebug() << this->lastQuery() << (GetTickCount() - start);
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

