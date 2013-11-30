#include <QStringList>

#include <windows.h>
#include <msi.h>

#include "app.h"

#include "..\wpmcpp\wpmutils.h"

App::App()
{
}

int App::listMSI()
{
    QStringList sl = WPMUtils::findInstalledMSIProductNames();

    WPMUtils::outputTextConsole("Installed MSI Products\n");
    for (int i = 0; i < sl.count(); i++) {
        WPMUtils::outputTextConsole(sl.at(i) + "\n");
    }

    return 0;
}

int App::process()
{
    cl.add("path", 'p',
            "directory path (e.g. C:\\Program Files (x86)\\MyProgram)",
            "path", false);
    cl.add("file", 'f',
            "path to an MSI package (e.g. C:\\Downloads\\MyProgram.msi)",
            "file", false);

    QString err = cl.parse();
    if (!err.isEmpty()) {
        WPMUtils::outputTextConsole("Error: " + err + "\n");
        return 1;
    }

    // cl.dump();

    int r = 0;

    QStringList fr = cl.getFreeArguments();

    if (fr.count() == 0) {
        help();
    } else if (fr.count() > 1) {
        WPMUtils::outputTextConsole("Unexpected argument: " + fr.at(1) + "\n", false);
        r = 1;
    } else if (fr.at(0) == "help") {
        help();
    } else if (fr.at(0) == "add-path") {
        r = addPath();
    } else if (fr.at(0) == "remove-path") {
        r = removePath();
    } else if (fr.at(0) == "list-msi") {
        r = listMSI();
    } else if (fr.at(0) == "get-product-code") {
        r = getProductCode();
    } else if (fr.at(0) == "uninstall-from-cp") {
        r = uninstallFromCP();
    } else {
        WPMUtils::outputTextConsole("Wrong command: " + fr.at(0) + "\n", false);
        r = 1;
    }

    return r;
}

int App::getProductCode()
{
    int ret = 0;

    QString file = cl.get("file");

    if (ret == 0) {
        if (file.isNull()) {
            WPMUtils::outputTextConsole("Missing option: --file\n", false);
            ret = 1;
        }
    }

    if (ret == 0) {
        MSIHANDLE hProduct;
        UINT r = MsiOpenPackageW((WCHAR*) file.utf16(), &hProduct);
        if (!r) {
            WCHAR guid[40];
            DWORD pcchValueBuf = 40;
            r = MsiGetProductPropertyW(hProduct, L"ProductCode", guid, &pcchValueBuf);
            if (!r) {
                QString s;
                s.setUtf16((ushort*) guid, pcchValueBuf);
                WPMUtils::outputTextConsole(s + "\n");
            } else {
                WPMUtils::outputTextConsole(
                        "Cannot get the value of the ProductCode property\n", false);
                ret = 1;
            }
        } else {
            WPMUtils::outputTextConsole("Cannot open the MSI file\n", false);
            ret = 1;
        }
    }

    return ret;
}

int App::uninstallFromCP()
{
    int ret = 0;

    QString label = cl.get("label");

    if (ret == 0) {
        if (label.isNull()) {
            WPMUtils::outputTextConsole("Missing option: --label\n", false);
            ret = 1;
        }
    }

    if (ret == 0) {
        MSIHANDLE hProduct;
        UINT r = MsiOpenPackageW((WCHAR*) label.utf16(), &hProduct);
        if (!r) {
            WCHAR guid[40];
            DWORD pcchValueBuf = 40;
            r = MsiGetProductPropertyW(hProduct, L"ProductCode", guid, &pcchValueBuf);
            if (!r) {
                QString s;
                s.setUtf16((ushort*) guid, pcchValueBuf);
                WPMUtils::outputTextConsole(s + "\n");
            } else {
                WPMUtils::outputTextConsole(
                        "Cannot get the value of the ProductCode property\n", false);
                ret = 1;
            }
        } else {
            WPMUtils::outputTextConsole("Cannot open the MSI file\n", false);
            ret = 1;
        }
    }

    return ret;
}

int App::help()
{
    const char* lines[] = {
        "CLU - Command line utility",
        "Usage:",
        "    clu [help]",
        "        prints this help",
        "    clu add-path --path=<path>",
        "        appends the specified path to the system-wide PATH variable",
        "    clu remove-path --path=<path>",
        "        removes the specified path from the system-wide PATH variable",
        "    clu list-msi",
        "        lists all installed MSI packages",
        "    clu get-product-code --file=<file>",
        "        prints the product code of an MSI file",
        "    clu uninstall-from-cp --label=<product label>",
        "        uninstalls a program according to its label in \"Add and remove software\" control panel",
        "Options:",
    };
    for (int i = 0; i < (int) (sizeof(lines) / sizeof(lines[0])); i++) {
        WPMUtils::outputTextConsole(QString(lines[i]) + "\n");
    }
    this->cl.printOptions();
    const char* lines2[] = {
        "",
        "The process exits with the code unequal to 0 if an error occcures.",
        "If the output is redirected, the texts will be encoded as UTF-8.",
    };
    for (int i = 0; i < (int) (sizeof(lines2) / sizeof(lines2[0])); i++) {
        WPMUtils::outputTextConsole(QString(lines2[i]) + "\n");
    }

    return 0;
}

int App::addPath()
{
    int r = 0;

    QString path = cl.get("path");

    if (r == 0) {
        if (path.isNull()) {
            WPMUtils::outputTextConsole("Missing option: --path\n", false);
            r = 1;
        }
    }

    if (r == 0) {
        if (path.contains(';')) {
            WPMUtils::outputTextConsole("The path cannot contain a semicolon\n",
                    false);
            r = 1;
        }
    }

    if (r == 0) {
        QString mpath = path.toLower().trimmed();
        mpath.replace('/', '\\');

        QString err;
        QString curPath = WPMUtils::getSystemEnvVar("PATH", &err);
        if (err.isEmpty()) {
            QStringList sl = curPath.split(';');
            bool found = false;
            for (int i = 0; i < sl.count(); i++) {
                QString s = sl.at(i).toLower().trimmed();
                s.replace('/', '\\');
                if (s == mpath) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (!curPath.endsWith(';'))
                    curPath += ';';
                curPath += path;

                // it's actually 2048, but Explorer seems to have a bug
                if (curPath.length() < 2040) {
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true);
                    if (!err.isEmpty()) {
                        r = 1;
                        WPMUtils::outputTextConsole(err + "\n", false);
                    } else {
                        WPMUtils::fireEnvChanged();
                    }
                } else {
                    r = 1;
                    WPMUtils::outputTextConsole(
                            "The new PATH value would be too long\n", false);
                }
            }
        } else {
            r = 1;
            WPMUtils::outputTextConsole(err + "\n", false);
        }
    }

    return r;
}

int App::removePath()
{
    int r = 0;

    QString path = cl.get("path");

    if (r == 0) {
        if (path.isNull()) {
            WPMUtils::outputTextConsole("Missing option: --path\n", false);
            r = 1;
        }
    }

    if (r == 0) {
        if (path.contains(';')) {
            WPMUtils::outputTextConsole("The path cannot contain a semicolon\n",
                    false);
            r = 1;
        }
    }

    if (r == 0) {
        QString mpath = path.toLower().trimmed();
        mpath.replace('/', '\\');

        QString err;
        QString curPath = WPMUtils::getSystemEnvVar("PATH", &err);
        if (err.isEmpty()) {
            QStringList sl = curPath.split(';');
            int index = -1;
            for (int i = 0; i < sl.count(); i++) {
                QString s = sl.at(i).toLower().trimmed();
                s.replace('/', '\\');
                if (s == mpath) {
                    index = i;
                    break;
                }
            }
            if (index >= 0) {
                sl.removeAt(index);
                curPath = sl.join(";");

                // it's actually 2048, but Explorer seems to have a bug
                if (curPath.length() < 2040) {
                    err = WPMUtils::setSystemEnvVar("PATH", curPath, true);
                    if (!err.isEmpty()) {
                        r = 1;
                        WPMUtils::outputTextConsole(err + "\n", false);
                    } else {
                        WPMUtils::fireEnvChanged();
                    }
                } else {
                    r = 1;
                    WPMUtils::outputTextConsole(
                            "The new PATH value would be too long\n", false);
                }
            }
        } else {
            r = 1;
            WPMUtils::outputTextConsole(err + "\n", false);
        }
    }

    return r;
}
