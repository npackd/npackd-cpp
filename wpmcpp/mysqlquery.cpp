#include "mysqlquery.h"

bool MySQLQuery::exec(const QString &query)
{
    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec(query);
    // qDebug() << query << (GetTickCount() - start);
    return r;
}

bool MySQLQuery::exec()
{
    //DWORD start = GetTickCount();
    bool r = QSqlQuery::exec();
    // qDebug() << this->lastQuery() << (GetTickCount() - start);
    return r;
}

