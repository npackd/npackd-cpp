#ifndef PACKAGE_H
#define PACKAGE_H

#include <QString>
#include <QStringList>
#include <QStringList>
#include <QMultiMap>
#include <QXmlStreamWriter>
#include <QJsonObject>

/**
 * A package declaration.
 *
 * Adding a field:
 * - add the member variable
 * - update toJSON
 * - update toXML
 */
class Package
{
public:
    /**
     * @brief searches for the specified object in the specified list. Objects
     *     will be compared only by package name.
     * @param pvs list of packages
     * @param f search for this object
     * @return index of the found object or -1
     */
    static int indexOf(const QList<Package*> pvs, Package* f);

    /* status of a package. The order of these constants is important. */
    enum Status {NOT_INSTALLED, INSTALLED, UPDATEABLE,
            NOT_INSTALLED_NOT_AVAILABLE};

    /** name of the package like "org.buggysoft.BuggyEditor" */
    QString name;

    /** short program title (1 line) like "Gimp" */
    QString title;

    /** web page associated with this piece of software. May be empty. */
    QString url;

    /** description. May contain multiple lines and paragraphs. */
    QString description;

    /** name of the license like "org.gnu.GPLv3" or "" if unknown */
    QString license;

    /** categories. Sub-categories are separated by / */
    QStringList categories;

    /** tags */
    QStringList tags;

    /**
     * <link> rel->href. The order of the values in QMultiMap is from
     * most recently to least recently inserted, but the appearance in XML is
     * in the other order.
     */
    QMultiMap<QString, QString> links;

    Package(const QString& name, const QString& title);

    /**
     * @brief getIcon
     * @return URL of the icon associated with this package. Only .png icons
     *     are currently supported
     */
    QString getIcon() const;

    /**
     * @brief returns the changelog URL
     * @return URL of the changelog or ""
     */
    QString getChangeLog() const;

    /**
     * @brief changes the URL of the changelog
     * @param icon URL or ""
     */
    void setChangeLog(const QString& changelog);

    /**
     * @brief changes the icon
     * @param icon URL or ""
     */
    void setIcon(const QString& icon);

    /**
     * Checks whether the specified value is a valid package name.
     *
     * @param a string that should be checked
     * @return true if name is a valid package name
     */
    static bool isValidName(const QString &name);

    /**
     * @param fullname full package name with "."
     * @return short name for this package. The short name contains only the
     *     part after the last dot.
     */
    static QString getShortName(const QString& fullname);

    /**
     * @return copy of this object
     */
    Package* clone() const;

    /**
     * @return short name for this package. The short name contains only the
     *     part after the last dot.
     */
    QString getShortName() const;

    /**
     * Stores this object as XML <package>.
     *
     * @param w output
     */
    void toXML(QXmlStreamWriter *w) const;

    /**
     * Stores this object as JSON.
     *
     * @param w output
     */
    void toJSON(QJsonObject &w) const;
};

#endif // PACKAGE_H
