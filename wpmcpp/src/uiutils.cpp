#include <windows.h>

#include <QMessageBox>
#include <QBuffer>
#include <qwinfunctions.h>

#include "uiutils.h"
#include "packageversion.h"

UIUtils::UIUtils()
{
}

QString UIUtils::extractIconURL(const QString& iconFile)
{
    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        UINT iconIndex = 0;
        int index = icon.lastIndexOf(',');
        if (index > 0) {
            QString number = icon.mid(index + 1);
            bool ok;
            int v = number.toInt(&ok);
            if (ok) {
                iconIndex = (UINT) v;
                icon = icon.left(index);
            }
        }

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                (LPCWSTR) icon.utf16(), iconIndex);
        if (((UINT_PTR) ic) > 1) {
            QPixmap pm = QtWin::fromHICON(ic);
            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QString("data:image/png;base64,") +
                        bytes.toBase64();
            }
            DestroyIcon(ic);
        }
    }

    return res;
}

QString UIUtils::createPackageVersionsHTML(const QStringList& names)
{
    QStringList allNames;
    if (names.count() > 10) {
        allNames = names.mid(0, 10);
        allNames.append("...");
    } else {
        allNames = names;
    }
    for (int i = 0; i < allNames.count(); i++) {
        allNames[i] = allNames[i].toHtmlEscaped();
    }
    return allNames.join("<br>");
}

bool UIUtils::confirmInstalOperations(QWidget* parent,
        QList<InstallOperation*> &install, QString* err)
{
    QList<PackageVersion*> pvs;

    // fetch package versions
    for (int j = 0; j < install.size(); j++) {
        InstallOperation* op = install.at(j);
        PackageVersion* pv = op->findPackageVersion(err);
        if (!err->isEmpty()) {
            *err = QObject::tr("Cannot find the package version %1: %2").arg(
                    pv->toString()).arg(*err);
            break;
        }
        pvs.append(pv);
    }

    // check for locked package versions
    if (err->isEmpty()) {
        for (int j = 0; j < pvs.size(); j++) {
            PackageVersion* pv = pvs.at(j);
            if (pv->isLocked()) {
                *err = QObject::tr("The package %1 is locked by a currently running installation/removal.").
                        arg(pv->toString());
                break;
            }
        }
    }

    // list of used package versions
    QList<bool> used;
    used.reserve(install.count());
    for (int i = 0; i < install.count(); i++) {
        used.append(false);
    }

    // find updates
    QStringList updateNames;
    if (err->isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            if (!op->install) {
                for (int j = 0; j < install.count(); j++) {
                    if (used[j] || j == i)
                        continue;

                    InstallOperation* op2 = install.at(j);
                    if (op2->install && op->package == op2->package &&
                            op->version < op2->version) {
                        used[i] = used[j] = true;

                        PackageVersion* pv = pvs.at(i);
                        updateNames.append(pv->toString() + " -> " +
                                op2->version.getVersionString());
                    }
                }
            }
        }
    }

    // find uninstalls
    QStringList uninstallNames;
    if (err->isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (!op->install) {
                uninstallNames.append(pv->toString());
            }
        }
    }

    // find installs
    QStringList installNames;
    if (err->isEmpty()) {
        for (int i = 0; i < install.count(); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (op->install) {
                installNames.append(pv->toString());
            }
        }
    }

    bool b = false;
    QString msg;
    QString title;
    QString dialogTitle;
    QString detailedMessage;
    if (installNames.count() == 1 && uninstallNames.count() == 0 &&
            updateNames.count() == 0) {
        b = true;
        title = QObject::tr("Installing");
    } else if (installNames.count() == 0 && uninstallNames.count() == 1 &&
            updateNames.count() == 0) {
        title = QObject::tr("Uninstalling");

        PackageVersion* pv = pvs.at(0);

        dialogTitle = QObject::tr("Uninstall");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("The package %1 will be uninstalled.").
                arg(pv->toString()).toHtmlEscaped() +
                "</h2>" +
                QObject::tr("The corresponding directory %1 will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                arg(pv->getPath()).toHtmlEscaped() +
                "</body>";
        detailedMessage = QObject::tr("The package %1 will be uninstalled.").
                arg(pv->toString());
    } else if (installNames.count() > 0 && uninstallNames.count() == 0 &&
            updateNames.count() == 0) {
        title = QString(QObject::tr("Installing %1 packages")).arg(
                installNames.count());
        dialogTitle = QObject::tr("Install");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(installNames) +
                "</b></body>";
        detailedMessage = QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()) + "\n" +
                installNames.join("\n");
    } else if (installNames.count() == 0 && uninstallNames.count() > 0 &&
            updateNames.count() == 0) {
        title = QString(QObject::tr("Uninstalling %1 packages")).arg(
                uninstallNames.count());
        dialogTitle = QObject::tr("Uninstall");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.count()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(uninstallNames) +
                "</b>.<br><br>" +
                QObject::tr("The corresponding directories will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                toHtmlEscaped() +
                "</body>";
        detailedMessage = QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.count()) + "\n" +
                uninstallNames.join("\n");
    } else {
        title = QObject::tr("Installing %1 packages, uninstalling %2 packages, updating %3 packages").
                arg(installNames.count()).
                arg(uninstallNames.count()).
                arg(updateNames.count());
        dialogTitle = QObject::tr("Install/Uninstall");
        msg = "<html><head/><body>";
        if (updateNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(updateNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.count()) + "\n" +
                    updateNames.join("\n");
        }
        if (uninstallNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(uninstallNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()) + "\n" +
                    uninstallNames.join("\n");
        }
        if (installNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(installNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\n\n";
            detailedMessage += QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()) + "\n" +
                    installNames.join("\n");
        }
        msg += "<br><br>" +
                QObject::tr("The corresponding directories will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                toHtmlEscaped();
        msg += "</body>";
    }

    if (err->isEmpty()) {
        if (!b)
            b = UIUtils::confirm(parent, dialogTitle, msg, detailedMessage);
    }

    qDeleteAll(pvs);
    pvs.clear();

    return b;
}

bool UIUtils::confirm(QWidget* parent, QString title, QString text,
        QString detailedText)
{
    /*
    QStringList lines = text.split("\n", QString::SkipEmptyParts);
    int max;
    if (lines.count() > 20) {
        max = 20;
    } else {
        max = lines.count();
    }
    int n = 0;
    int keep = 0;
    for (int i = 0; i < max; i++) {
        n += lines.at(i).length();
        if (n > 20 * 80)
            break;
        keep++;
    }
    if (keep == 0)
        keep = 1;
    QString shortText;
    if (keep < lines.count()) {
        lines = lines.mid(0, keep);
        shortText = lines.join("\n") + "...";
    } else {
        shortText = text;
    }
    */

    QMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setText(text);
    mb.setIcon(QMessageBox::Warning);
    mb.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    mb.setDefaultButton(QMessageBox::Ok);
    mb.setDetailedText(detailedText);
    return ((QMessageBox::StandardButton) mb.exec()) == QMessageBox::Ok;
}

