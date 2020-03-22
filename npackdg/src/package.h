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
 * - update package.cpp
 * - add a field to PackageItemModel::Info
 * - update PackageItemModel.cpp
 * - update MainFrame::MainFrame
 * - update RepositoryXMLHandler
 * - add field in the database (DBRepository.cpp)
 * - add the field to the package detail frame
 */
class Package
{
public:
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

    /** number of users that starred this package */
    int stars;

    Package(const Package& p) = default;

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
     * @param changelog URL or ""
     */
    void setChangeLog(const QString& changelog);

    /**
     * @brief returns the issue tracker URL
     * @return URL of the issue tracker or ""
     */
    QString getIssueTracker() const;

    /**
     * @brief changes the URL of the issue tracker
     * @param v URL or ""
     */
    void setIssueTracker(const QString& v);

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
