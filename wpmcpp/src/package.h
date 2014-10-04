#ifndef PACKAGE_H
#define PACKAGE_H

#include <QString>
#include <QStringList>
#include <QDomElement>
#include <QStringList>

/**
 * A package declaration.
 */
class Package
{
public:
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

    /** icon URL or "" */
    QString icon;

    /** description. May contain multiple lines and paragraphs. */
    QString description;

    /** name of the license like "org.gnu.GPLv3" or "" if unknown */
    QString license;

    /** PACKAGE.REPOSITORY. ID of the repisotory. */
    int repository;

    /** categories. Sub-categories are separated by / */
    QStringList categories;

    /** changelog URL */
    QString changelog;

    /** https: or http: URLs to screenshots in PNG format */
    QStringList screenshots;

    Package(const QString& name, const QString& title);

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
};

#endif // PACKAGE_H
