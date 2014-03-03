#ifndef MYSQLQUERY_H
#define MYSQLQUERY_H

#include <QSqlQuery>

class MySQLQuery: public QSqlQuery {
public:
    bool exec(const QString& query);
    bool exec();
    bool next();
    bool prepare(const QString &query);
};


#endif // MYSQLQUERY_H
