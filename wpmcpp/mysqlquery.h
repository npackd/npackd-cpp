#ifndef MYSQLQUERY_H
#define MYSQLQUERY_H

#include <QSqlQuery>

class MySQLQuery: public QSqlQuery {
public:
    bool exec(const QString& query);
    bool exec();
};


#endif // MYSQLQUERY_H
