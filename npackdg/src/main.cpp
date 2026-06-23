#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <QApplication>
#include <QMetaType>
#include <QTranslator>
#include <QVBoxLayout>
#include <QImageReader>
#include <QtPlugin>
#include <QBuffer>
#include <QStyleFactory>

#include "version.h"
#include "mainwindow.h"
#include "repository.h"
#include "wpmutils.h"
#include "job.h"
#include "fileloader.h"
#include "abstractrepository.h"
#include "dbrepository.h"
#include "installedpackages.h"
#include "commandline.h"
#include "packageversion.h"
#include "installoperation.h"
#include "uiutils.h"
#include "clprocessor.h"
#include "uimessagehandler.h"
#include "packageutils.h"

// These lines are necessary for a static build
#ifdef NPACKD_STATIC
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin);
Q_IMPORT_PLUGIN(QICNSPlugin);
Q_IMPORT_PLUGIN(QICOPlugin);
Q_IMPORT_PLUGIN(QJpegPlugin);
Q_IMPORT_PLUGIN(QGifPlugin);
Q_IMPORT_PLUGIN(QTiffPlugin);
Q_IMPORT_PLUGIN(QWbmpPlugin);
Q_IMPORT_PLUGIN(QWebpPlugin);
Q_IMPORT_PLUGIN(QSvgPlugin);
Q_IMPORT_PLUGIN(QTgaPlugin);
#endif

// Qt 5.2.1
// qtbase/src/gui/image/qpixmap_win.cpp:qt_pixmapFromWinHICON has a bug.
// It crashes in the following line sometimes:
//     return QPixmap::fromImage(image);
// => this function only uses QImage
static QImage my_qt_pixmapFromWinHICON(HICON icon)
{
    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 0";

    HDC screenDevice = GetDC(0);
    HDC hdc = CreateCompatibleDC(screenDevice);
    ReleaseDC(0, screenDevice);

    ICONINFO iconinfo;
    bool result = GetIconInfo(icon, &iconinfo);
    if (!result)
        qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 1";

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 2" << w << h;

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 3";

    HBITMAP winBitmap = CreateDIBSection(hdc, &bmi,
            DIB_RGB_COLORS, (void**) &bits, 0, 0);
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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 4";

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

    // qCDebug(npackd) << "my_qt_pixmapFromWinHICON 5";

    return image;
}

QString extractIconURL_(const QString& iconFile)
{
    // qCDebug(npackd) << "extractIconURL 0";

    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        // qCDebug(npackd) << "extractIconURL 1";

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

        // qCDebug(npackd) << "extractIconURL 2" << icon << iconIndex;

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                WPMUtils::toLPWSTR(icon), iconIndex);

        // qCDebug(npackd) << "extractIconURL 2.1" << ((UINT_PTR) ic);

        if (((UINT_PTR) ic) > 1) {
            // qCDebug(npackd) << "extractIconURL 3" << ii << info.fIcon << info.xHotspot;

            QImage pm = my_qt_pixmapFromWinHICON(ic); // QtWin::fromHICON(ic);

            // qCDebug(npackd) << "extractIconURL 4";

            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                // qCDebug(npackd) << "extractIconURL 5";

                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QStringLiteral("data:image/png;base64,") +
                        bytes.toBase64();

                // qCDebug(npackd) << "extractIconURL 6";
            }
            DestroyIcon(ic);

            // qCDebug(npackd) << "extractIconURL 7";
        }
    }

    return res;
}

int main(int argc, char *argv[])
{
    //qCDebug(npackd) << QUrl("file:///C:/test").resolved(QUrl::fromLocalFile("abc.txt"));

    qInstallMessageHandler(eventLogMessageHandler);

    HMODULE m = LoadLibrary(L"exchndl.dll");

    QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

    WPMUtils::extractIconURL = extractIconURL_;

#if NPACKD_ADMIN != 1
    PackageUtils::globalMode = WPMUtils::hasAdminPrivileges();
#endif

    // this does not work unfortunately. The only way to ensure the maximum width of a tooltip is to
    // use HTML
    // a.setStyleSheet("QToolTip { height: auto; max-width: 400px; color: #ffffff; background-color: #2a82da; border: 1px solid white; }");

    QString packageName;
#if !defined(__x86_64__) && !defined(_WIN64)
    packageName = "com.googlecode.windows-package-manager.Npackd";
#else
    packageName = "com.googlecode.windows-package-manager.Npackd64";
#endif
    InstalledPackages::packageName = packageName;

    qRegisterMetaType<Version>("Version");
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

#if !defined(__x86_64__) && !defined(_WIN64)
    if (WPMUtils::is64BitWindows()) {
        QMessageBox::critical(0, "Error",
                QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\r\n" +
                QObject::tr("Please download the 64 bit version from https://github.com/tim-lebedkov/npackd/wiki/Downloads"));
        return 1;
    }
#endif


    // July, 25 2018:
    // "bmp", "cur", "gif", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "xbm", "xpm"
    // December, 25 2018:
    // "bmp", "cur", "gif", "icns", "ico", "jp2", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // July, 27 2019:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // September, 26 2020:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
    // January, 28 2024:
    // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm" 
    qCInfo(npackdImportant) << QImageReader::supportedImageFormats();

    //QString::fromLatin1(QImageReader::supportedImageFormats().join(','));
    //QMessageBox::information(0, "Error", "def"); //ba));

    // July, 25 2018: "windowsvista", "Windows", "Fusion"
    qCDebug(npackd) << QStyleFactory::keys();

    CLProcessor clp;

    int errorCode;
    clp.process(argc, argv, &errorCode);

    //WPMUtils::timer.dump();

    FreeLibrary(m);

    return errorCode;
}
