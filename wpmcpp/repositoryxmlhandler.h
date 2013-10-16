#ifndef REPOSITORYXMLHANDLER_H
#define REPOSITORYXMLHANDLER_H

#include <QXmlDefaultHandler>
#include <QString>
#include <QXmlAttributes>

#include "license.h"
#include "package.h"
#include "packageversion.h"
#include "abstractrepository.h"
#include "dbrepository.h"

/**
 * @brief SAX handler for the repository XML.
 */
class RepositoryXMLHandler: public QXmlDefaultHandler
{
    DBRepository* rep;

    License* lic;
    Package* p;
    PackageVersion* pv;
    PackageVersionFile* pvf;
    Dependency* dep;
    DetectFile* df;

    QString chars;
    QString error;
    QStringList tags;

    bool isIn(const QString& first, const QString& second) const;
    bool isIn(const QString& first, const QString& second,
            const QString& third) const;
    bool isIn(const QString& first, const QString& second,
            const QString& third, const QString& fourth) const;
public:
    /**
     * -
     *
     * @param rep [owner:caller] data will be stored here
     */
    RepositoryXMLHandler(DBRepository* rep);

    virtual ~RepositoryXMLHandler();

    bool startElement(const QString& namespaceURI,
            const QString& localName, const QString& qName,
            const QXmlAttributes& atts);
    bool endElement(const QString& namespaceURI,
            const QString& localName, const QString& qName);
    bool characters(const QString& ch);
    bool fatalError(const QXmlParseException& exception);
    QString errorString() const;
};

#endif // REPOSITORYXMLHANDLER_H
