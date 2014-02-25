#ifndef WINDOWSREGISTRY_H
#define WINDOWSREGISTRY_H

#include "windows.h"

#include "qstring.h"

/**
 * Windows registry. Better than QSettings as the values do not land under
 * Wow6432Node on 64-bit Windows versions.
 */
class WindowsRegistry
{
private:
    /** current key or 0 */
    HKEY hkey;

    bool useWow6432Node;
    REGSAM samDesired;
public:
    /**
     * Creates an uninitialized object.
     */
    WindowsRegistry();

    /**
     * Creates an object from an existing HKEY
     *
     * @param hk existing HKEY or 0
     * @param useWow6432Node if true, Wow6432Node node is used under 64-bit
     *     Windows
     * @param samDesired access flags
     */
    WindowsRegistry(HKEY hk, bool useWow6432Node,
            REGSAM samDesired = KEY_ALL_ACCESS);

    /**
     * Creates a copy
     *
     * @param wr other registry object
     */
    WindowsRegistry(const WindowsRegistry& wr);

    ~WindowsRegistry();

    WindowsRegistry& operator=(const WindowsRegistry& wr);

    /**
     * @brief saves a list of values
     * @param values the values
     * @return error message
     */
    QString saveStringList(const QStringList& values) const;

    /**
     * Delete a sub-key without children.
     *
     * @param name name of the sub-key (cannot contain \)
     * @return error message or ""
     */
    QString remove(const QString& name) const;

    /**
     * Reads a REG_SZ value.
     *
     * @param name name of the variable. Use "" for the default value.
     * @param err error message will be stored here
     * @return the value
     */
    QString get(QString name, QString* err) const;

    /**
     * Changes permissions for this key so that everybody can read it.
     *
     * @return error message or ""
     */
    // unused QString allowReadAccessToEverybody();

    /**
     * Reads a DWORD value.
     *
     * @param name name of the variable
     * @param err error message will be stored here
     * @return the value
     */
    DWORD getDWORD(QString name, QString* err) const;

    /**
     * Writes a DWORD value.
     *
     * @param name name of the variable
     * @return error message
     * @param value the value
     */
    QString setDWORD(QString name, DWORD value) const;

    /**
     * Reads a BINARY value.
     *
     * @param name name of the variable
     * @param err error message will be stored here
     * @return the value
     */
    QByteArray getBytes(QString name, QString* err) const;

    /**
     * Writes a BINARY value.
     *
     * @param name name of the variable
     * @return error message
     * @param value the value
     */
    QString setBytes(QString name, const QByteArray &value) const;

    /**
     * Writes a REG_SZ value.
     *
     * @param name name of the variable
     * @param value the value
     * @return error message
     */
    QString set(QString name, QString value) const;

    /**
     * Writes a REG_EXPAND_SZ value.
     *
     * @param name name of the variable
     * @param value the value
     * @return error message
     */
    QString setExpand(QString name, QString value) const;

    /**
     * Opens a key. The previously open key (if any) will be closed.
     *
     * @param hk a key
     * @param path path under hk
     * @param useWow6432Node if true, Wow6432Node is used on 64-bit Windows
     * @param samDesired access flags
     * @param e error code returned by RegOpenKeyEx will be stored here. This
     *     can be 0.
     * @return error message or ""     
     */
    QString open(HKEY hk, QString path, bool useWow6432Node,
            REGSAM samDesired = KEY_ALL_ACCESS, LONG* e = 0);

    /**
     * Opens a key. The previously open key (if any) will be closed.
     *
     * @param wr points to a node in the registry
     * @param subkey name of the subkey (may contain \)
     * @param samDesired access flags
     * @return error message or ""
     */
    QString open(const WindowsRegistry& wr, QString subkey,
            REGSAM samDesired = KEY_ALL_ACCESS);

    /**
     * Opens or creates a sub-key.
     *
     * @param name name of the sub-key
     * @param err error message or ""
     * @param samDesired access flags
     * @return created key (uninitialized, if an error occured)
     */
    WindowsRegistry createSubKey(QString name, QString* err,
            REGSAM samDesired = KEY_ALL_ACCESS) const;

    /**
     * Closes the current key.
     *
     * @return error message or ""
     */
    QString close();

    /**
     * @param err the error message will be stored here
     * @return list of sub-keys
     */
    QStringList list(QString* err) const;

    /**
     * @param err the error message will be stored here
     * @return list of values
     */
    QStringList listValues(QString* err) const;

    /**
     * @brief loads QStringList from this key
     * @param err error message
     * @return data from the registry
     */
    QStringList loadStringList(QString *err) const;

    /**
     * Delete a sub-key recursively.
     *
     * @param name name of the sub-key (cannot contain \)
     * @return error message or ""
     */
    QString removeRecursively(const QString &name) const;
};

#endif // WINDOWSREGISTRY_H
