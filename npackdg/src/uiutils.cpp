#include <windows.h>

#include <QtGlobal>
#if defined(_MSC_VER) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include <QObject>
#include <QMessageBox>
#include <QBuffer>
#include <qwinfunctions.h>
#include <QPixmap>
#include <QLabel>
#include <QAbstractButton>
#include <QtConcurrent/QtConcurrent>

#include "uiutils.h"
#include "packageversion.h"
#include "mainwindow.h"
#include "abstractrepository.h"
#include "job.h"
#include "wpmutils.h"

UIUtils::UIUtils()
{
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

bool UIUtils::confirmInstallOperations(QWidget* parent,
        QList<InstallOperation*> &install, QString* title, QString* err)
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
    QString dialogTitle;
    QString detailedMessage;
    if (installNames.count() == 1 && uninstallNames.count() == 0 &&
            updateNames.count() == 0) {
        b = true;
        *title = QObject::tr("Installing %1").arg(installNames.at(0));
    } else if (installNames.count() == 0 && uninstallNames.count() == 1 &&
            updateNames.count() == 0) {
        *title = QObject::tr("Uninstalling %1").arg(uninstallNames.at(0));

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
        *title = QString(QObject::tr("Installing %1 packages")).arg(
                installNames.count());
        dialogTitle = QObject::tr("Install");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(installNames) +
                "</b></body>";
        detailedMessage = QObject::tr("%1 package(s) will be installed:").
                arg(installNames.count()) + "\r\n" +
                installNames.join("\r\n");
    } else if (installNames.count() == 0 && uninstallNames.count() > 0 &&
            updateNames.count() == 0) {
        *title = QString(QObject::tr("Uninstalling %1 packages")).arg(
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
                arg(uninstallNames.count()) + "\r\n" +
                uninstallNames.join("\r\n");
    } else {
        *title = "";

        if (installNames.count() > 0) {
            *title += QObject::tr("installing %1 packages").
                    arg(installNames.count());
        }
        if (uninstallNames.count() > 0) {
            *title += ", " + QObject::tr("uninstalling %1 packages").
                    arg(uninstallNames.count());
        }
        if (updateNames.count() > 0) {
            *title += ", " + QObject::tr("updating %1 packages").
                    arg(updateNames.count());
        }
        if (title->startsWith(", "))
            title->remove(0, 2);
        *title = QObject::tr("Process") + ": " + *title;

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
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.count()) + "\r\n" +
                    updateNames.join("\r\n");
        }
        if (uninstallNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(uninstallNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.count()) + "\r\n" +
                    uninstallNames.join("\r\n");
        }
        if (installNames.count() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(installNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.count()) + "\r\n" +
                    installNames.join("\r\n");
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
        shortText = lines.join("\r\n") + "...";
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
    return static_cast<QMessageBox::StandardButton>(mb.exec()) ==
            QMessageBox::Ok;
}

void UIUtils::processWithSelfUpdate(Job* job,
        QList<InstallOperation*> &ops, DWORD programCloseType)
{
    QString newExe;

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.8, "Copying the executable");

        QString thisExe = WPMUtils::getExeFile();

        // 1. copy .exe to the temporary directory
        QTemporaryFile of(QDir::tempPath() + "\\npackdgXXXXXX.exe");
        of.setAutoRemove(false);
        if (!of.open())
            job->setErrorMessage(of.errorString());
        else {
            newExe = of.fileName();
            if (!of.remove()) {
                job->setErrorMessage(of.errorString());
            } else {
                of.close();

                // qCDebug(npackd) << "self-update 1";

                if (!QFile::copy(thisExe, newExe))
                    job->setErrorMessage("Error copying the binary");
                else
                    sub->completeWithProgress();

                // qCDebug(npackd) << "self-update 2";
            }
        }
    }

    QString batchFileName;
    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1, "Creating the .bat file");

        QString pct = WPMUtils::programCloseType2String(programCloseType);
        QStringList batch;
        for (int i = 0; i < ops.count(); i++) {
            InstallOperation* op = ops.at(i);
            QString oneCmd = "\"" + newExe + "\" ";

            // ping 1.1.1.1 always fails so we use || instead of &&
            if (op->install) {
                oneCmd += "add -p " + op->package + " -v " +
                        op->version.getVersionString() +
                        " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
            } else {
                oneCmd += "remove -p " + op->package + " -v " +
                        op->version.getVersionString() +
                        " -e " + pct +
                        " || ping 1.1.1.1 -n 1 -w 10000 > nul || exit /b %errorlevel%";
            }
            batch.append(oneCmd);
        }

        batch.append("\"" + newExe + "\" start-newest");

        // qCDebug(npackd) << "self-update 3";

        QTemporaryFile file(QDir::tempPath() +
                          "\\npackdgXXXXXX.bat");
        file.setAutoRemove(false);
        if (!file.open())
            job->setErrorMessage(file.errorString());
        else {
            batchFileName = file.fileName();

            // qCDebug(npackd) << "batch" << file.fileName();

            QTextStream stream(&file);
            stream.setCodec("UTF-8");
            stream << batch.join("\r\n");
            if (stream.status() != QTextStream::Ok)
                job->setErrorMessage("Error writing the .bat file");
            file.close();

            // qCDebug(npackd) << "self-update 4";

            sub->completeWithProgress();
        }
    }

    if (job->shouldProceed()) {
        Job* sub = job->newSubJob(0.1, "Starting the copied binary");

        QString file_ = batchFileName;
        file_.replace('/', '\\');
        QString prg = WPMUtils::findCmdExe();
        QString winDir = WPMUtils::getWindowsDir();

        QString args = "\"" + prg + "\" /U /E:ON /V:OFF /C \"\"" + file_ + "\"\"";

        PROCESS_INFORMATION pinfo;

        STARTUPINFOW startupInfo = {
            sizeof(STARTUPINFO), nullptr, nullptr, nullptr,
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            static_cast<DWORD>(CW_USEDEFAULT),
            0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr
        };
        bool success = CreateProcess(
                WPMUtils::toLPWSTR(prg),
                WPMUtils::toLPWSTR(args),
                nullptr, nullptr, TRUE,
                CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, nullptr,
                WPMUtils::toLPWSTR(winDir), &startupInfo, &pinfo);

        if (success) {
            CloseHandle(pinfo.hThread);
            CloseHandle(pinfo.hProcess);
            // qCDebug(npackd) << "success!222";
        }

        // qCDebug(npackd) << "self-update 5";

        sub->completeWithProgress();
        job->setProgress(1);
    }

    job->complete();
}

QString UIUtils::extractAccelerators(const QStringList& titles)
{
    QString valid = "abcdefghijklmnopqrstuvwxyz";

    QString used;
    for (int i = 0; i < titles.count(); i++) {
        QString title = titles.at(i);

        int pos = title.indexOf('&');
        if (pos >= 0 && pos + 1 < title.length()) {
            QChar c = title.at(pos + 1).toLower();
            if (valid.contains(c)) {
                if (!used.contains(c))
                    used.append(c);
                else
                    title.remove(pos, 1);
            }
        }
    }

    return used;
}

void UIUtils::chooseAccelerators(QStringList* titles, const QString& ignore)
{
    QString valid = "abcdefghijklmnopqrstuvwxyz";

    QString used = ignore;
    for (int i = 0; i < titles->count(); i++) {
        QString title = titles->at(i);

        if (title.contains('&')) {
            int pos = title.indexOf('&');
            if (pos + 1 < title.length()) {
                QChar c = title.at(pos + 1).toLower();
                if (valid.contains(c)) {
                    if (!used.contains(c))
                        used.append(c);
                    else
                        title.remove(pos, 1);
                }
            }
        }

        if (!title.contains('&')) {
            QString s = title.toLower();
            int pos = -1;
            for (int j = 0; j < s.length(); j++) {
                QChar c = s.at(j);
                if (valid.contains(c) && !used.contains(c)) {
                    pos = j;
                    break;
                }
            }

            if (pos >= 0) {
                QChar c = s.at(pos);
                used.append(c);
                title.insert(pos, '&');
            }
        }

        titles->replace(i, title);
    }
}

void UIUtils::chooseAccelerators(QWidget *w, const QString& ignore)
{
    QList<QWidget*> widgets = w->findChildren<QWidget*>();

    QList<QWidget*> used;
    QStringList usedTexts;
    for (int i = 0; i < widgets.count(); i++) {
        QWidget* w = widgets.at(i);

        QLabel *label = dynamic_cast<QLabel*>(w);
        if (label) {
            used.append(label);
            usedTexts.append(label->text());
            continue;
        }

        QAbstractButton *button= dynamic_cast<QAbstractButton*>(w);
        if (button) {
            used.append(button);
            usedTexts.append(button->text());
            continue;
        }
    }

    chooseAccelerators(&usedTexts, ignore);

    for (int i = 0; i < used.count(); i++) {
        QWidget* w = used.at(i);

        QLabel *label = dynamic_cast<QLabel*>(w);
        if (label) {
            label->setText(usedTexts.at(i));
            continue;
        }

        QAbstractButton *button= dynamic_cast<QAbstractButton*>(w);
        if (button) {
            button->setText(usedTexts.at(i));
            continue;
        }
    }
}

