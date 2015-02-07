#ifndef PACKAGE_H
#define PACKAGE_H

#include <QString>
#include <QStringList>
#include <QDomElement>
#include <QStringList>
#include <QMultiMap>
#include <QXmlStreamWriter>

/**
 * A package declaration.
 */
class Package
{
public:
    /**
     * @param e <package>
     * @param err error message will be stored here
     * @param validate true = perform all available validations
     * @return created object or 0
     */
    static Package* parse(QDomElement* e, QString* err,
            bool validate=true);

    /**
     * @brief searches for a package only using the package name
     * @param list searching in this list
     * @param pv searching for this package
     * @return true if the list contains the specified package
     */
    static bool contains(const QList<Package*>& list, Package* pv);

    /**
     * @brief searches for the specified object in the specified list. Objects
     *     will be compared only by package name.
     * @param pvs list of packages
     * @param f search for this object
     * @return index of the found object or -1
     */
    static int indexOf(const QList<Package*> pvs, Package* f);

    /* status of a package */
    enum Status {NOT_INSTALLED, INSTALLED, UPDATEABLE};

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

    /** PACKAGE.REPOSITORY. ID of the repisotory. */
    int repository;

    /** categories. Sub-categories are separated by / */
    QStringList categories;

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
     * Checks whether the specified value is a valid URL that can be used
     * for example as the home page value.
     *
     * @param a string that should be checked
     * @return true if name is a valid URL
     */
    static bool isValidURL(const QString &url);

    /**
     * Save the contents as XML.
     *
     * @param e <package>
     */
    void saveTo(QDomElement& e) const;

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
};

#endif // PACKAGE_H
