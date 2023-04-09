#ifndef REPOSITORYXMLHANDLER_H
#define REPOSITORYXMLHANDLER_H

#include <QXmlDefaultHandler>
#include <QString>
#include <QXmlAttributes>
#include <QXmlStreamReader>

#include "license.h"
#include "package.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"

/**
 * @brief SAX handler for the repository XML.
 */
class RepositoryXMLHandler
{
    AbstractRepository* rep;

    QXmlStreamReader* reader;

    QUrl url;

    int findWhere();
public:
    /**
     * -
     *
     * @param rep [move] data will be stored here
     * @param url this value will be used for resolving relative URLs. This can
     *     be an empty URL. In this case relative URLs are not allowed.
     * @param reader XML reader
     */
    RepositoryXMLHandler(AbstractRepository* rep, const QUrl& url, QXmlStreamReader* reader);

    virtual ~RepositoryXMLHandler();

    /**
     * @brief parse
     * @return error message or ""
     */
    QString parse();

    /**
     * @brief parse "version"
     * @return error message or ""
     */
    QString parseTopLevelVersion();
private:
    /**
     * @brief parses inside of "root"
     * @return error message or ""
     */
    QString parseRoot();
    void parseVersion();
    void parsePackage();
    void parseLicense();
    QString formatError();
};

#endif // REPOSITORYXMLHANDLER_H
