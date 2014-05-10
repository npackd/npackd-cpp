#include <windows.h>

#include <QDebug>
#include <QMessageBox>
#include <QBuffer>
#include <qwinfunctions.h>
#include <QPixmap>

#include "uiutils.h"
#include "packageversion.h"

UIUtils::UIUtils()
{
}

// Qt 5.2.1
// qtbase/src/gui/image/qpixmap_win.cpp:qt_pixmapFromWinHICON has a bug.
// It crashes in the following line sometimes:
//     return QPixmap::fromImage(image);
// => this function only uses QImage
QImage my_qt_pixmapFromWinHICON(HICON icon)
{
    // qDebug() << "my_qt_pixmapFromWinHICON 0";

    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    bool result = GetIconInfo(icon, &iconinfo);
    if (!result)
        qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

    // qDebug() << "my_qt_pixmapFromWinHICON 1";

    int w = 0;
    int h = 0;
    if (!iconinfo.xHotspot || !iconinfo.yHotspot) {
        // We could not retrieve the icon size via GetIconInfo,
        // so we try again using the icon bitmap.
        BITMAP bm;
        int result = GetObject(iconinfo.hbmColor, sizeof(BITMAP), &bm);
        if (!result) result = GetObject(iconinfo.hbmMask, sizeof(BITMAP), &bm);
        if (!result) {
            qWarning("QPixmap::fromWinHICON(), failed to retrieve icon size");
            return QImage();
        }
        w = bm.bmWidth;
        h = bm.bmHeight;
    } else {
        // x and y Hotspot describes the icon center
        w = iconinfo.xHotspot * 2;
        h = iconinfo.yHotspot * 2;
    }
    const DWORD dwImageSize = w * h * 4;

    // qDebug() << "my_qt_pixmapFromWinHICON 2" << w << h;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFO);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = dwImageSize;

    uchar* bits;

    // qDebug() << "my_qt_pixmapFromWinHICON 3";

    HBITMAP winBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &bits, 0, 0);
    if (winBitmap )
        memset(bits, 0xff, dwImageSize);
    if (!winBitmap) {
        qWarning("QPixmap::fromWinHICON(), failed to CreateDIBSection()");
        return QImage();
    }

    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_NORMAL))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");

    uint mask = 0xff000000;
    // Create image and copy data into image.
    QImage image(w, h, QImage::Format_ARGB32);

    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                dest[x] = src[x];
            }
        }
    }

    // qDebug() << "my_qt_pixmapFromWinHICON 4";

    if (!DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK))
        qWarning("QPixmap::fromWinHICON(), failed to DrawIcon()");
    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y < h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (bits + y * bytes_per_line);
            for (int x=0; x < w; ++x) {
                if (!src[x])
                    dest[x] = dest[x] | mask;
            }
        }
    }

    SelectObject(hdc, oldhdc); //restore state
    DeleteObject(winBitmap);
    DeleteDC(hdc);

    // qDebug() << "my_qt_pixmapFromWinHICON 5";

    return image;
}

QString UIUtils::extractIconURL(const QString& iconFile)
{
    // qDebug() << "extractIconURL 0";

    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        // qDebug() << "extractIconURL 1";

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

        // qDebug() << "extractIconURL 2" << icon << iconIndex;

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                (LPCWSTR) icon.utf16(), iconIndex);

        // qDebug() << "extractIconURL 2.1" << ((UINT_PTR) ic);

        if (((UINT_PTR) ic) > 1) {
            // qDebug() << "extractIconURL 3" << ii << info.fIcon << info.xHotspot;

            QImage pm = my_qt_pixmapFromWinHICON(ic); // QtWin::fromHICON(ic);

            // qDebug() << "extractIconURL 4";

            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                // qDebug() << "extractIconURL 5";

                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QString("data:image/png;base64,") +
                        bytes.toBase64();

                // qDebug() << "extractIconURL 6";
            }
            DestroyIcon(ic);

            // qDebug() << "extractIconURL 7";
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

bool UIUtils::confirmInstallOperations(QWidget* parent,
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

