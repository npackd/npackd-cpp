#include "sqlutils.h"

#include <QSqlError>
#include <QVariant>
#include <QSqlRecord>

#include "mysqlquery.h"

SQLUtils::SQLUtils()
{

}

QString SQLUtils::toString(const QSqlError& e)
{
    return e.type() == QSqlError::NoError ? QStringLiteral("") : e.text();
}

QString SQLUtils::getErrorString(const MySQLQuery &q)
{
    QSqlError e = q.lastError();
    return e.type() == QSqlError::NoError ? QStringLiteral("") : e.text() +
            QStringLiteral(" (") + q.lastQuery() + QStringLiteral(")");
}

bool SQLUtils::tableExists(QSqlDatabase* db,
        const QString& table, QString* err)
{
    *err = QStringLiteral("");

    MySQLQuery q(*db);
    if (!q.prepare(QStringLiteral("SELECT name FROM sqlite_master WHERE "
            "type='table' AND name=:NAME")))
        *err = getErrorString(q);

    if (err->isEmpty()) {
        q.bindValue(QStringLiteral(":NAME"), table);
        if (!q.exec())
            *err = getErrorString(q);
    }

    bool e = false;
    if (err->isEmpty()) {
        e = q.next();
    }

    return e;
}

bool SQLUtils::columnExists(QSqlDatabase* db,
        const QString& table, const QString& column, QString* err)
{
    *err = QStringLiteral("");

    MySQLQuery q(*db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(") + table +
            QStringLiteral(")")))
        *err = getErrorString(q);

    bool e = false;
    if (err->isEmpty()) {
        int index = q.record().indexOf(QStringLiteral("name"));
        while (q.next()) {
            QString n = q.value(index).toString();
            if (QString::compare(n, column, Qt::CaseInsensitive) == 0) {
                e = true;
                break;
            }
        }
    }

    return e;
}

