#ifndef VERSION_H
#define VERSION_H

#include "qstring.h"

class Version
{
private:
    const static int BASIC_PARTS = 4;

    /**
     * this is used instead of allocating memory on the heap for performance.
     * Version numbers with more than 4 parts are still stored on the heap.
     */
    int basic[BASIC_PARTS];

    /** may point to *basic* */
    int* parts;

    int nparts;
public:
    static const Version EMPTY;

    /**
     * Initialization with 1.0
     */
    Version();

    /**
     * Initialization with a.b
     *
     * @param a major version number
     * @param b minor version number
     */
    Version(int a, int b);

    /**
     * @param v "1.2.3"
     */
    Version(const QString& v);

    Version(const Version& v);

    Version& operator =(const Version& v);

    bool operator !=(const Version& v) const;
    bool operator ==(const Version& v) const;
    bool operator <(const Version& v) const;
    bool operator <=(const Version& v) const;
    bool operator >(const Version& v) const;

    ~Version();

    /**
     * Changes the version
     *
     * @param version "1.2.3"
     * @return true if it was a valid version. The internal value is not changed
     *     if a not-valid version was supplied
     */
    bool setVersion(const QString& version);

    /**
     * Changes the version.
     *
     * @param a first version number
     * @param b second (minor) version number part
     */
    void setVersion(int a, int b);

    /**
     * Changes the version.
     *
     * @param a first version number
     * @param b second version number part
     * @param c third (minor) version number part
     */
    void setVersion(int a, int b, int c);

    /**
     * Changes the version.
     *
     * @param a first version number
     * @param b second version number part
     * @param c third version number part
     * @param d 4th (minor) version number part
     */
    void setVersion(int a, int b, int c, int d);

    /**
     * @return package version as a string (like "1.2.3")
     */
    QString getVersionString() const;

    /**
     * @param nparts number of parts in the returned version
     * @return package version as a string (like "1.2")
     */
    QString getVersionString(int nparts) const;

    /**
     * Prepends a number before the version.
     * Example: Version("1.2").prepend(5) => "5.1.2"
     *
     * @param number this number will be prepended
     */
    void prepend(int number);

    /**
     * Compares this version with another one.
     *
     * @param other other version
     * @return <0, 0 or >0
     */
    int compare(const Version& other) const;

    /**
     * @return number of parts in this version number. Returns 1 for the
     *     version "0"
     */
    int getNParts() const;

    /**
     * Normalizes this object by cutting the trailing zeros. For example "1.2.0"
     * will be changed to "1.2"
     */
    void normalize();

    /**
     * @return true if this version number is normalized (does not contain
     *     trailing zeros)
     */
    bool isNormalized() const;
};

#endif // VERSION_H
