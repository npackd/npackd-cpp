#ifndef MYSQLQUERY_H
#define MYSQLQUERY_H

#include <QSqlQuery>
#include <QSqlDatabase>

class MySQLQuery: public QSqlQuery {
public:
    explicit MySQLQuery(QSqlDatabase db);
    bool exec(const QString& query);
    bool exec();
    bool next();
    bool prepare(const QString &query);
};


#endif // MYSQLQUERY_H
