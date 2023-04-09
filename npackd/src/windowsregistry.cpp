#include <shlwapi.h>
#include <windows.h>
#include <aclapi.h>

#include <QString>

#include "windowsregistry.h"
#include "wpmutils.h"

WindowsRegistry::WindowsRegistry()
{
    this->hkey = nullptr;
    this->useWow6432Node = false;
    this->samDesired = KEY_ALL_ACCESS;
}

WindowsRegistry::WindowsRegistry(const WindowsRegistry& wr)
{
    this->hkey = nullptr;
    this->useWow6432Node = false;
    this->samDesired = KEY_ALL_ACCESS;
    *this = wr;
}

WindowsRegistry::WindowsRegistry(HKEY hk, bool useWow6432Node,
        REGSAM samDesired)
{
    this->hkey = hk;
    this->useWow6432Node = useWow6432Node;
    this->samDesired = samDesired;
}

WindowsRegistry::~WindowsRegistry()
{
    close();
}

WindowsRegistry& WindowsRegistry::operator=(const WindowsRegistry& wr)
{
    if (this != &wr) {
        close();

        this->useWow6432Node = wr.useWow6432Node;
        this->samDesired = wr.samDesired;

        if (wr.hkey == nullptr)
            this->hkey = nullptr;
        else
            this->hkey = SHRegDuplicateHKey(wr.hkey);
    }

    // to support chained assignment operators (a=b=c), always return *this
    return *this;
}

std::vector<QString> WindowsRegistry::loadStringList(QString* err) const
{
    std::vector<QString> r;

    *err = "";
    int n = getDWORD("", err);
    if (err->isEmpty()) {
        for (int i = 0; i < n; i++) {
            QString v = get(QString::number(i), err);
            if (!err->isEmpty())
                break;
            r.push_back(v);
        }
    }

    return r;
}

QString WindowsRegistry::saveStringList(const std::vector<QString> &values) const
{
    QString err = setDWORD("", values.size());

    if (err.isEmpty()) {
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            err = set(QString::number(i), values.at(i));
            if (!err.isEmpty())
                break;
        }
    }

    return err;
}

QString WindowsRegistry::get(const QString &name, QString* err) const
{
    err->clear();

    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return "";
    }

    QString value_;
    DWORD valueSize;
    BYTE small_[256];
    valueSize = sizeof(small_);
    LONG r = RegQueryValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), nullptr, nullptr, small_,
                &valueSize);

    // valueSize may be 0!
    if (r == ERROR_MORE_DATA) {
        if (valueSize != 0) {
            char* value = new char[valueSize];
            r = RegQueryValueEx(this->hkey,
                        WPMUtils::toLPWSTR(name), nullptr, nullptr, (BYTE*) value,
                        &valueSize);
            if (r != ERROR_SUCCESS) {
                WPMUtils::formatMessage(r, err);
            } else {
                value_.setUtf16((ushort*) value, valueSize / 2 - 1);
            }
            delete[] value;
        }
    } else if (r == ERROR_SUCCESS) {
        if (valueSize != 0) {
            value_.setUtf16((ushort*) small_, valueSize / 2 - 1);
        }
    } else {
        WPMUtils::formatMessage(r, err);
    }

    return value_;
}

DWORD WindowsRegistry::getDWORD(const QString &name, QString* err) const
{
    err->clear();

    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return 0;
    }

    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    DWORD type;
    LONG r = RegQueryValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), nullptr, &type, (BYTE*) &value,
                &valueSize);
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, err);
    } else if (type != REG_DWORD) {
        *err = QObject::tr("Wrong registry value type (DWORD expected)");
    }
    return value;
}

QString WindowsRegistry::setDWORD(const QString &name, DWORD value) const
{
    QString err;

    if (this->hkey == nullptr) {
        return QObject::tr("No key is open");
    }

    DWORD valueSize = sizeof(value);
    LONG r = RegSetValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), 0, REG_DWORD, (BYTE*) &value,
                valueSize);
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, &err);
    }
    return err;
}

QByteArray WindowsRegistry::getBytes(const QString &name, QString *err) const
{
    err->clear();

    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return nullptr;
    }

    QByteArray value;
    DWORD valueSize = 0;
    DWORD type;
    LONG r = RegQueryValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), nullptr, &type, (BYTE*) value.data(),
                &valueSize);
    if (type != REG_BINARY) {
        *err = QObject::tr("Wrong registry value type (BINARY expected)");
    } else if (r == ERROR_MORE_DATA) {
        value.resize(valueSize);
        LONG r = RegQueryValueEx(this->hkey,
                    WPMUtils::toLPWSTR(name), nullptr, &type, (BYTE*) value.data(),
                    &valueSize);
        if (r != ERROR_SUCCESS) {
            WPMUtils::formatMessage(r, err);
        }
    } else {
        WPMUtils::formatMessage(r, err);
    }
    return value;
}

QString WindowsRegistry::setBytes(const QString &name, const QByteArray& value) const
{
    QString err;

    if (this->hkey == nullptr) {
        err = QObject::tr("No key is open");
    } else {
        DWORD valueSize = value.length();
        LONG r = RegSetValueEx(this->hkey,
                    WPMUtils::toLPWSTR(name), 0, REG_BINARY, (BYTE*) value.data(),
                    valueSize);
        if (r != ERROR_SUCCESS) {
            WPMUtils::formatMessage(r, &err);
        }
    }

    return err;
}

QString WindowsRegistry::set(const QString &name, const QString &value) const
{
    QString err;

    if (this->hkey == nullptr) {
        return QObject::tr("No key is open");
    }

    DWORD valueSize = (value.length() + 1) * 2;
    LONG r = RegSetValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), 0, REG_SZ, (BYTE*) value.utf16(),
                valueSize);
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, &err);
    }
    return err;
}

QString WindowsRegistry::setExpand(const QString &name, const QString &value) const
{
    QString err;

    if (this->hkey == nullptr) {
        return QObject::tr("No key is open");
    }

    DWORD valueSize = (value.length() + 1) * 2;
    LONG r = RegSetValueEx(this->hkey,
                WPMUtils::toLPWSTR(name), 0, REG_EXPAND_SZ, (BYTE*) value.utf16(),
                valueSize);
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, &err);
    }
    return err;
}

std::vector<QString> WindowsRegistry::list(QString* err) const
{
    err->clear();

    std::vector<QString> res;
    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return res;
    }

    WCHAR name[255];
    int index = 0;
    while (true) {
        DWORD nameSize = sizeof(name) / sizeof(name[0]);
        LONG r = RegEnumKeyEx(this->hkey, index, name, &nameSize,
                nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_SUCCESS) {
            QString v_;
            v_.setUtf16((ushort*) name, nameSize);
            res.push_back(v_);
        } else if (r == ERROR_NO_MORE_ITEMS) {
            break;
        } else {
            WPMUtils::formatMessage(r, err);
            break;
        }
        index++;
    }

    return res;
}

std::vector<QString> WindowsRegistry::listValues(QString *err) const
{
    err->clear();

    std::vector<QString> res;
    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return res;
    }

    int nameReserved = 255;
    WCHAR* name = new WCHAR[nameReserved];

    int index = 0;
    while (true) {
        DWORD nameSize = nameReserved;
        LONG r = RegEnumValue(this->hkey, index, name, &nameSize,
                nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_SUCCESS) {
            QString v_;
            v_.setUtf16((ushort*) name, nameSize);
            res.push_back(v_);
            index++;
        } else if (r == ERROR_NO_MORE_ITEMS) {
            break;
        } else if (r == ERROR_MORE_DATA) {
            delete[] name;
            nameReserved = nameSize;
            name = new WCHAR[nameReserved];
        } else {
            WPMUtils::formatMessage(r, err);
            break;
        }
    }

    delete[] name;

    return res;
}

QString WindowsRegistry::open(const WindowsRegistry& wr, const QString &subkey,
        REGSAM samDesired)
{
    return open(wr.hkey, subkey, wr.useWow6432Node, samDesired);
}

QString WindowsRegistry::open(HKEY hk, const QString &path, bool useWow6432Node,
        REGSAM samDesired, LONG *e)
{
    QString ret;

    LONG error = ERROR_SUCCESS;

    close();

    this->useWow6432Node = useWow6432Node;
    this->samDesired = samDesired;

    if (WPMUtils::is64BitWindows()) {
        samDesired = samDesired |
                (useWow6432Node ? KEY_WOW64_32KEY : KEY_WOW64_64KEY);
    }

    error = RegOpenKeyEx(hk,
            WPMUtils::toLPWSTR(path),
            0, samDesired,
            &this->hkey);
    if (error != ERROR_SUCCESS) {
        QString s;
        WPMUtils::formatMessage(error, &s);
        ret = QObject::tr("Error opening registry node %1, using WOW6432 node: %2: %3").
                arg(path).arg(useWow6432Node).arg(s);
    }

    if (e)
        *e = error;

    return ret;
}

WindowsRegistry WindowsRegistry::createSubKey(const QString &name, QString* err,
        REGSAM samDesired) const
{
    err->clear();

    if (this->hkey == nullptr) {
        err->append(QObject::tr("No key is open"));
        return WindowsRegistry();
    }

    REGSAM sd = samDesired;
    if (WPMUtils::is64BitWindows()) {
        sd = sd | (useWow6432Node ? KEY_WOW64_32KEY : KEY_WOW64_64KEY);
    }

    HKEY hk;
    LONG r = RegCreateKeyEx(this->hkey,
            WPMUtils::toLPWSTR(name),
            0, nullptr, 0, sd, nullptr,
            &hk, nullptr);
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, err);
        hk = nullptr;
    }

    return WindowsRegistry(hk, this->useWow6432Node, samDesired);
}

QString WindowsRegistry::close()
{
    if (this->hkey != nullptr) {
        LONG r = RegCloseKey(this->hkey);
        this->hkey = nullptr;
        if (r == ERROR_SUCCESS) {
            return "";
        } else {
            QString s;
            WPMUtils::formatMessage(r, &s);
            return s;
        }
    } else {
        return "";
    }
}

QString WindowsRegistry::remove(const QString& name) const
{
    QString result;
    LONG r = RegDeleteKeyW(this->hkey, WPMUtils::toLPWSTR(name));
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        WPMUtils::formatMessage(r, &result);
        result = QObject::tr("Error removing registry node %1: %2").
                arg(name, result);
    }
    return result;
}

QString WindowsRegistry::removeRecursively(const QString& name) const
{
    QString result;
    LONG r = SHDeleteKeyW(this->hkey, WPMUtils::toLPWSTR(name));
    if (r != ERROR_SUCCESS) {
        WPMUtils::formatMessage(r, &result);
    }
    return result;
}
