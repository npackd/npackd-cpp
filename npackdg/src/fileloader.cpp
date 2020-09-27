#include <windows.h>
#include <shlobj.h>

#include <QtGlobal>
#if defined(_MSC_VER) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "fileloader.h"
#include "downloader.h"
#include "job.h"
#include "concurrent.h"
#include "wpmutils.h"
#include "sqlutils.h"

extern HWND defaultPasswordWindow;

QThreadPool FileLoader::threadPool;
FileLoader::_init FileLoader::_initializer;

FileLoader::FileLoader(): dbr(nullptr), id(0)
{
}

void FileLoader::watcherSizeFinished()
{
    QFutureWatcher<URLInfo>* w = static_cast<
            QFutureWatcher<URLInfo>*>(sender());

    URLInfo r = w->result();

    this->mutex.lock();
    dbr->saveURLSize(r.address, r.size);
    this->mutex.unlock();

    this->mutex.lock();
    URLInfo* v = this->sizes.value(r.address);
    if (!v) {
        v = new URLInfo(r.address);
        this->sizes.insert(r.address, v);
    }
    v->size = r.size;
    v->sizeModified = r.sizeModified;
    this->mutex.unlock();

    emit this->downloadSizeCompleted(r.address, r.size);

    w->deleteLater();
}

int64_t FileLoader::downloadSizeOrQueue(const QString &url)
{
    int64_t r;

    this->mutex.lock();
    URLInfo* v = this->sizes.value(url);
    if (v) {
        r = v->size;

        // if the value is older than 10 days, it is considered obsolete
        if (r >= 0 && time(nullptr) - v->sizeModified > 10 * 24 * 60 * 60) {
            r = -1;
        }
    } else {
        r = -1;
    }
    this->mutex.unlock();

    if (r == -1) {
        this->mutex.lock();
        QFuture<URLInfo> future = run(&threadPool, this,
                &FileLoader::downloadSizeRunnable, url);
        QFutureWatcher<URLInfo>* w =
                new QFutureWatcher<URLInfo>(this);
        connect(w, SIGNAL(finished()), this, SLOT(watcherFinished()));
        w->setFuture(future);
        this->mutex.unlock();
    }

    return r;
}

URLInfo FileLoader::downloadSizeRunnable(
        const QString& url)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    /*
    makes the process too slow
    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);
    */

    CoInitialize(nullptr);

    URLInfo r(url);

    this->mutex.lock();
    if (!this->dbr) {
        dbr = new DBRepository();
        QString err = dbr->openDefault("defaultDownloadSizeFinder");
        qCDebug(npackd) << "DownloadSizeFinder::downloadRunnable.openDefault" << err;
    }
    this->mutex.unlock();

    this->mutex.lock();
    URLInfo* v = this->sizes.value(url);
    if (v) {
        r = *v;
    } else {
        QString err;
        std::tie(r, err) = dbr->findURLInfo(url);
        if (err.isEmpty() && r.size >= 0) {
            v = new URLInfo(r);
            this->sizes.insert(r.address, v);
        }
    }
    this->mutex.unlock();

    // if the value is older than 10 days, it is considered obsolete
    if (r.size >= 0 && time(nullptr) - r.sizeModified > 10 * 24 * 60 * 60) {
        r.size = -1;
    }

    if (r.size < 0) {
        Job* job = new Job();
        r.size = Downloader::getContentLength(job, url, defaultPasswordWindow);
        r.sizeModified = time(nullptr);

        if (!job->getErrorMessage().isEmpty() || r.size < 0) {
            r.size = -2;
        }

        delete job;
    }

    CoUninitialize();

    /*
    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    */

    return r;
}

QString FileLoader::init()
{
    QString dir = WPMUtils::getShellDir(CSIDL_LOCAL_APPDATA) +
            QStringLiteral("\\Npackd\\Cache");
    QDir d;
    if (!d.exists(dir))
        d.mkpath(dir);

    QString path = dir + QStringLiteral("\\Data.db");

    path = QDir::toNativeSeparators(path);

    const QString connectionName = "FileLoader";

    QSqlDatabase::removeDatabase(connectionName);
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(path);

    qCDebug(npackd) << "before opening the database";
    db.open();
    qCDebug(npackd) << "after opening the database";

    QString err = SQLUtils::toString(db.lastError());

    if (err.isEmpty())
        err = exec(QStringLiteral("PRAGMA busy_timeout = 30000"));

    if (err.isEmpty()) {
        err = exec(QStringLiteral("PRAGMA journal_mode = DELETE"));
    }

    if (err.isEmpty()) {
        err = exec(QStringLiteral("PRAGMA case_sensitive_like = on"));
    }

    if (err.isEmpty()) {
        err = updateDatabase();
    }

    return err;
}

FileLoader::~FileLoader()
{
    qDeleteAll(this->sizes);
    this->sizes.clear();
    delete this->dbr;
}

QString FileLoader::exec(const QString& sql)
{
    QMutexLocker ml(&this->mutex);

    MySQLQuery q(db);
    q.exec(sql);
    QString err = SQLUtils::getErrorString(q);

    return err;
}

QString FileLoader::downloadFileOrQueue(const QString &url, QString *err)
{
    QString r;
    *err = "";

    this->mutex.lock();
    if (this->files.contains(url)) {
        DownloadFile file = this->files.value(url);
        if (!file.error.isEmpty())
            *err = file.error;
        else if (!file.file.isEmpty())
            r = dir.path() + "\\" + file.file;
        this->mutex.unlock();
    } else {
        this->mutex.unlock();

        QFuture<DownloadFile> future = run(
                &FileLoader::threadPool, this,
                &FileLoader::downloadFileRunnable, url);
        QFutureWatcher<DownloadFile>* w =
                new QFutureWatcher<DownloadFile>(this);
        connect(w, SIGNAL(finished()), this,
                SLOT(watcherFinished()));
        w->setFuture(future);
    }

    return r;
}

void FileLoader::watcherFileFinished()
{
    QFutureWatcher<DownloadFile>* w = static_cast<
            QFutureWatcher<DownloadFile>*>(sender());
    DownloadFile r = w->result();

    if (!r.file.isEmpty() || !r.error.isEmpty()) {
        QString file = r.file;
        if (!file.isEmpty())
            file = dir.path() + "\\" + file;

        emit this->downloadFileCompleted(r.url, file, r.error);
    }

    w->deleteLater();
}

FileLoader::DownloadFile FileLoader::downloadFileRunnable(const QString& url)
{
    FileLoader::DownloadFile r;
    r.url = url;

    this->mutex.lock();
    if (this->files.contains(url)) {
        r = this->files.value(url);
        this->mutex.unlock();
    } else {
        this->files.insert(url, r);
        this->mutex.unlock();

        QThread::currentThread()->setPriority(QThread::LowestPriority);

        bool b = SetThreadPriority(GetCurrentThread(),
                THREAD_MODE_BACKGROUND_BEGIN);

        CoInitialize(nullptr);

        QString filename = QString::number(id.fetchAndAddAcquire(1));
        QString fn = dir.path() + "\\" + filename;
        QFile f(fn);
        if (f.open(QFile::ReadWrite)) {
            Job* job = new Job();
            Downloader::Request request{QUrl(url)};
            request.file = &f;
            request.useCache = true;
            request.keepConnection = false;
            request.timeout = 15;

            Downloader::Response response = Downloader::download(job, request);
            QString mime = response.mimeType;
            f.close();

            if (!job->getErrorMessage().isEmpty()) {
                r.error = job->getErrorMessage();
            } else {
                // see main.cpp for the supported extensions.
                QString ext;
                if (mime == "image/png")
                    ext = ".png";
                else if (mime == "image/x-icon" || mime == "image/vnd.microsoft.icon")
                    ext = ".ico";
                else if (mime == "image/jpeg")
                    ext = ".jpg";
                else if (mime == "image/gif")
                    ext = ".gif";
                else if (mime == "image/x-windows-bmp" || mime == "image/bmp")
                    ext = ".bmp";
                else if (mime == "image/svg+xml")
                    ext = ".svg";
                else
                    ext = ".png";

                // qCDebug(npackd) << ext;
                f.close();
                f.rename(fn + ext);
                r.file = filename + ext;
            }
            delete job;
        } else {
            r.error = QObject::tr("Cannot open the file %1").arg(fn);
        }

        this->mutex.lock();
        this->files.insert(r.url, r);
        this->mutex.unlock();

        CoUninitialize();

        if (b)
            SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    }

    return r;
}

QString FileLoader::updateDatabase()
{
    QString err;

    bool e = false;

    // URL. This table is new in Npackd 1.27.
    if (err.isEmpty()) {
        e = SQLUtils::tableExists(&db, "URL", &err);
    }
    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE TABLE URL("
                    "ADDRESS TEXT NOT NULL PRIMARY KEY, "
                    "SIZE INTEGER, "
                    "SIZE_MODIFIED INTEGER, "
                    "FILE TEXT)");
            err = SQLUtils::toString(db.lastError());
        }
    }

    return err;
}
