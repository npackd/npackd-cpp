// This file imports static plugin classes.
#include <QtPlugin>

// this is necessary in Qt 5.11 and earlier versions or for the static build
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0) || QT_LINK_STATIC == 1
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_PLUGIN(QICNSPlugin)
    Q_IMPORT_PLUGIN(QICOPlugin)
    Q_IMPORT_PLUGIN(QJpegPlugin)
    Q_IMPORT_PLUGIN(QGifPlugin)
    Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
    Q_IMPORT_PLUGIN(QTgaPlugin)
    Q_IMPORT_PLUGIN(QTiffPlugin)
    Q_IMPORT_PLUGIN(QWbmpPlugin)
    Q_IMPORT_PLUGIN(QWebpPlugin)
    Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
#endif
