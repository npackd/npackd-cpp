#include <windows.h>

#include <QtGlobal>

#include <QObject>
#include <QMessageBox>
#include <QBuffer>
#include <qwinfunctions.h>
#include <QPixmap>
#include <QLabel>
#include <QAbstractButton>

#include "uiutils.h"
#include "packageversion.h"
#include "mainwindow.h"
#include "abstractrepository.h"
#include "job.h"
#include "wpmutils.h"

UIUtils::UIUtils()
{
}

QString UIUtils::createPackageVersionsHTML(const std::vector<QString>& names)
{
    std::vector<QString> allNames;
    if (names.size() > 10) {
        allNames.insert(allNames.end(), names.begin(), names.begin() + 10);
        allNames.push_back("...");
    } else {
        allNames = names;
    }
    for (int i = 0; i < static_cast<int>(allNames.size()); i++) {
        allNames[i] = allNames[i].toHtmlEscaped();
    }
    return WPMUtils::join(allNames, "<br>");
}

bool UIUtils::confirmInstallOperations(QWidget* parent,
        std::vector<InstallOperation*> &install, QString* title, QString* err)
{
    std::vector<PackageVersion*> pvs;

    // fetch package versions
    for (auto op: install) {
        PackageVersion* pv = op->findPackageVersion(err);
        if (!err->isEmpty()) {
            *err = QObject::tr("Cannot find the package version %1: %2").arg(
                    pv->toString(), *err);
            break;
        }
        pvs.push_back(pv);
    }

    // check for locked package versions
    if (err->isEmpty()) {
        for (auto pv: pvs) {
            if (pv->isLocked()) {
                *err = QObject::tr("The package %1 is locked by a currently running installation/removal.").
                        arg(pv->toString());
                break;
            }
        }
    }

    // list of used package versions
    std::vector<bool> used;
    used.reserve(install.size());
    for (int i = 0; i < static_cast<int>(install.size()); i++) {
        used.push_back(false);
    }

    // find updates
    std::vector<QString> updateNames;
    if (err->isEmpty()) {
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            if (!op->install) {
                for (int j = 0; j < static_cast<int>(install.size()); j++) {
                    if (used[j] || j == i)
                        continue;

                    InstallOperation* op2 = install.at(j);
                    if (op2->install && op->package == op2->package &&
                            op->version < op2->version) {
                        used[i] = used[j] = true;

                        PackageVersion* pv = pvs.at(i);
                        updateNames.push_back(pv->toString() + " -> " +
                                op2->version.getVersionString());
                    }
                }
            }
        }
    }

    // find uninstalls
    std::vector<QString> uninstallNames;
    if (err->isEmpty()) {
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (!op->install) {
                uninstallNames.push_back(pv->toString());
            }
        }
    }

    // find installs
    std::vector<QString> installNames;
    if (err->isEmpty()) {
        for (int i = 0; i < static_cast<int>(install.size()); i++) {
            if (used[i])
                continue;

            InstallOperation* op = install.at(i);
            PackageVersion* pv = pvs.at(i);
            if (op->install) {
                installNames.push_back(pv->toString());
            }
        }
    }

    bool b = false;
    QString msg;
    QString dialogTitle;
    QString detailedMessage;
    if (installNames.size() == 1 && uninstallNames.size() == 0 &&
            updateNames.size() == 0) {
        b = true;
        *title = QObject::tr("Installing %1").arg(installNames.at(0));
    } else if (installNames.size() == 0 && uninstallNames.size() == 1 &&
            updateNames.size() == 0) {
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
    } else if (installNames.size() > 0 && uninstallNames.size() == 0 &&
            updateNames.size() == 0) {
        *title = QString(QObject::tr("Installing %1 packages")).arg(
                installNames.size());
        dialogTitle = QObject::tr("Install");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be installed:").
                arg(installNames.size()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(installNames) +
                "</b></body>";
        detailedMessage = QObject::tr("%1 package(s) will be installed:").
                arg(installNames.size()) + "\r\n" +
                WPMUtils::join(installNames, "\r\n");
    } else if (installNames.size() == 0 && uninstallNames.size() > 0 &&
            updateNames.size() == 0) {
        *title = QString(QObject::tr("Uninstalling %1 packages")).arg(
                uninstallNames.size());
        dialogTitle = QObject::tr("Uninstall");
        msg = QString("<html><head/><body><h2>") +
                QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.size()).toHtmlEscaped() +
                "</h2><br><b>" +
                createPackageVersionsHTML(uninstallNames) +
                "</b>.<br><br>" +
                QObject::tr("The corresponding directories will be completely deleted. There is no way to restore the files. The processes locking the files will be closed.").
                toHtmlEscaped() +
                "</body>";
        detailedMessage = QObject::tr("%1 package(s) will be uninstalled:").
                arg(uninstallNames.size()) + "\r\n" +
                WPMUtils::join(uninstallNames, "\r\n");
    } else {
        *title = "";

        if (installNames.size() > 0) {
            *title += QObject::tr("installing %1 packages").
                    arg(installNames.size());
        }
        if (uninstallNames.size() > 0) {
            *title += ", " + QObject::tr("uninstalling %1 packages").
                    arg(uninstallNames.size());
        }
        if (updateNames.size() > 0) {
            *title += ", " + QObject::tr("updating %1 packages").
                    arg(updateNames.size());
        }
        if (title->startsWith(", "))
            title->remove(0, 2);
        *title = QObject::tr("Process") + ": " + *title;

        dialogTitle = QObject::tr("Install/Uninstall");
        msg = "<html><head/><body>";
        if (updateNames.size() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.size()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(updateNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%3 package(s) will be updated:").
                    arg(updateNames.size()) + "\r\n" +
                    WPMUtils::join(updateNames, "\r\n");
        }
        if (uninstallNames.size() > 0) {
            msg += "<h2>" +
                    QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.size()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(uninstallNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%1 package(s) will be uninstalled:").
                    arg(uninstallNames.size()) + "\r\n" +
                    WPMUtils::join(uninstallNames, "\r\n");
        }
        if (installNames.size() > 0) {
            msg += "<h2>" +
                    QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.size()).toHtmlEscaped() +
                    "</h2><br><b>" +
                    createPackageVersionsHTML(installNames) +
                    "</b>";
            if (!detailedMessage.isEmpty())
                detailedMessage += "\r\n\r\n";
            detailedMessage += QObject::tr("%3 package(s) will be installed:").
                    arg(installNames.size()) + "\r\n" +
                    WPMUtils::join(installNames, "\r\n");
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
    std::vector<QString> lines = text.split("\n", QString::SkipEmptyParts);
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
        std::vector<InstallOperation*> &ops, DWORD programCloseType)
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
        std::vector<QString> batch;
        for (auto op: ops) {
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
            batch.push_back(oneCmd);
        }

        batch.push_back("\"" + newExe + "\" start-newest");

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
            stream << WPMUtils::join(batch, "\r\n");
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

QString UIUtils::extractAccelerators(const std::vector<QString> &titles)
{
    QString valid = "abcdefghijklmnopqrstuvwxyz";

    QString used;
    for (int i = 0; i < static_cast<int>(titles.size()); i++) {
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

void UIUtils::chooseAccelerators(std::vector<QString>* titles, const QString& ignore)
{
    QString valid = "abcdefghijklmnopqrstuvwxyz";

    QString used = ignore;
    for (int i = 0; i < static_cast<int>(titles->size()); i++) {
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

        titles->at(i) = title;
    }
}

void UIUtils::chooseAccelerators(QWidget *w, const QString& ignore)
{
    QList<QWidget*> widgets = w->findChildren<QWidget*>();

    std::vector<QWidget*> used;
    std::vector<QString> usedTexts;
    for (int i = 0; i < widgets.count(); i++) {
        QWidget* w = widgets.at(i);

        QLabel *label = dynamic_cast<QLabel*>(w);
        if (label) {
            used.push_back(label);
            usedTexts.push_back(label->text());
            continue;
        }

        QAbstractButton *button= dynamic_cast<QAbstractButton*>(w);
        if (button) {
            used.push_back(button);
            usedTexts.push_back(button->text());
            continue;
        }
    }

    chooseAccelerators(&usedTexts, ignore);

    for (int i = 0; i < static_cast<int>(used.size()); i++) {
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

