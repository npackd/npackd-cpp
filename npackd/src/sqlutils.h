#ifndef SQLUTILS_H
#define SQLUTILS_H

#include <QSqlDatabase>
#include <QString>

#include "mysqlquery.h"

/**
 * @brief SQL utilities
 */
class SQLUtils
{
private:
    SQLUtils();
public:
    /**
     * @brief get the error message from an SQL query
     *
     * @param q query
     * @return error message
     */
    static QString getErrorString(const MySQLQuery& q);

    /**
     * @brief checks if a table exists
     *
     * @param db database
     * @param table table name
     * @param err error message will be stored here
     * @return true = the table exists
     */
    static bool tableExists(QSqlDatabase* db,
            const QString& table, QString* err);

    /**
     * @brief checks if a table column exists
     *
     * @param db database
     * @param table table name
     * @param column column name
     * @param err error message will be stored here
     * @return true = the table exists
     */
    static bool columnExists(QSqlDatabase *db, const QString &table,
                             const QString &column, QString *err);

    /**
     * @brief convert QSqlError to a string
     *
     * @param e error
     * @return error message
     */
    static QString toString(const QSqlError &e);
};

#endif // SQLUTILS_H
