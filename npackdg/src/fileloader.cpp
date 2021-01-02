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

FileLoader::FileLoader()
{
}

std::tuple<int64_t, QString> FileLoader::saveURLInfos(
        const QString& url, int64_t size,
        const QString& file)
{
    this->mutex.lock();

    QString err;
    int64_t id = 0;

    if (!updateURLQuery) {
        updateURLQuery.reset(new MySQLQuery(db));

        QString sql("UPDATE URL SET SIZE=:SIZE, SIZE_MODIFIED=:SIZE_MODIFIED, FILE=:FILE WHERE ADDRESS=:ADDRESS");

        if (!updateURLQuery->prepare(sql)) {
            err = SQLUtils::getErrorString(*updateURLQuery);
            updateURLQuery.reset(nullptr);
        }
    }

    bool insert = false;

    if (err.isEmpty()) {
        updateURLQuery->bindValue(QStringLiteral(":ADDRESS"), url);
        updateURLQuery->bindValue(QStringLiteral(":SIZE"), size);
        updateURLQuery->bindValue(QStringLiteral(":SIZE_MODIFIED"),
                static_cast<qlonglong>(time(nullptr)));
        updateURLQuery->bindValue(QStringLiteral(":FILE"), file);
        if (!updateURLQuery->exec())
            err = SQLUtils::getErrorString(*updateURLQuery);
        if (updateURLQuery->numRowsAffected() == 0)
            insert = true;
        updateURLQuery->finish();
    }

    if (err.isEmpty() && !insertURLInfosQuery) {
        insertURLInfosQuery = new MySQLQuery(db);

        QString insertSQL("INSERT INTO URL"
                "(ADDRESS, SIZE, SIZE_MODIFIED, FILE) "
                "VALUES(:ADDRESS, :SIZE, :SIZE_MODIFIED, :FILE)");

        if (!insertURLInfosQuery->prepare(insertSQL)) {
            err = SQLUtils::getErrorString(*insertURLInfosQuery);
            delete insertURLInfosQuery;
        }
    }

    if (err.isEmpty() && insert) {
        insertURLInfosQuery->bindValue(QStringLiteral(":ADDRESS"), url);
        insertURLInfosQuery->bindValue(QStringLiteral(":SIZE"), size);
        insertURLInfosQuery->bindValue(QStringLiteral(":SIZE_MODIFIED"),
                static_cast<qlonglong>(time(nullptr)));
        insertURLInfosQuery->bindValue(QStringLiteral(":FILE"), file);
        if (!insertURLInfosQuery->exec())
            err = SQLUtils::getErrorString(*insertURLInfosQuery);

        id = insertURLInfosQuery->lastInsertId().toLongLong();
        insertURLInfosQuery->finish();
    }

    this->mutex.unlock();

    if (err.isEmpty() && !insert) {
        DownloadFile df;
        QString err;
        std::tie(df, err) = findURLInfo(url);
        id = df.id;
    }

    return std::tie(id, err);
}

void FileLoader::watcherSizeFinished()
{
    QFutureWatcher<DownloadFile>* w = static_cast<
            QFutureWatcher<DownloadFile>*>(sender());

    DownloadFile r = w->result();

    saveURLInfos(r.url, r.size, r.file);

    emit this->downloadSizeCompleted(r.url, r.size);

    w->deleteLater();
}

int64_t FileLoader::downloadSizeOrQueue(const QString &url)
{
    int64_t r = -1;

    DownloadFile v;
    QString err;
    std::tie(v, err) = this->findURLInfo(url);

    r = v.size;

    // if the value is older than 10 days, it is considered obsolete
    if (r == -1 || time(nullptr) - v.sizeModified > 10 * 24 * 60 * 60) {
        this->mutex.lock();
        QFuture<DownloadFile> future = run(&threadPool, this,
                &FileLoader::downloadSizeRunnable, url);
        QFutureWatcher<DownloadFile>* w =
                new QFutureWatcher<DownloadFile>(this);
        connect(w, SIGNAL(finished()), this, SLOT(watcherSizeFinished()));
        w->setFuture(future);
        this->mutex.unlock();
    }

    return r;
}

FileLoader::DownloadFile FileLoader::downloadSizeRunnable(
        const QString& url)
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);

    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);

    CoInitialize(nullptr);

    Job* job = new Job();
    DownloadFile v;
    v.url = url;
    v.size = Downloader::getContentLength(job, url, defaultPasswordWindow);
    v.sizeModified = time(nullptr);

    if (!job->getErrorMessage().isEmpty() || v.size < 0) {
        v.size = -2;
    }

    delete job;

    CoUninitialize();

    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);

    return v;
}

QString FileLoader::init()
{
    dir = WPMUtils::getShellDir(CSIDL_LOCAL_APPDATA) +
            QStringLiteral("\\Npackd\\Cache");
    QDir d;
    if (!d.exists(dir + "\\Files"))
        d.mkpath(dir + "\\Files");

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
    delete insertURLInfosQuery;
}

std::tuple<FileLoader::DownloadFile, QString>
        FileLoader::findURLInfo(const QString& url)
{
    QMutexLocker ml(&this->mutex);

    QString err = "";
    DownloadFile info;
    info.url = url;
    info.size = -1;

    MySQLQuery q(db);
    if (!q.prepare(QStringLiteral(
            "SELECT ID, SIZE, SIZE_MODIFIED, FILE FROM URL WHERE ADDRESS = :ADDRESS LIMIT 1")))
        err = SQLUtils::getErrorString(q);

    if (npackd().isDebugEnabled()) {
        qCDebug(npackd) << url;
    }

    if (err.isEmpty()) {
        q.bindValue(QStringLiteral(":ADDRESS"), url);
        if (!q.exec())
            err = SQLUtils::getErrorString(q);
    }

    if (err.isEmpty()) {
        if (q.next()) {
            info.id = q.value(0).toLongLong();
            info.size = q.value(1).toLongLong();
            info.sizeModified = q.value(2).toLongLong();
            info.file = q.value(3).toString();
        } else {
            err = "Not found";
        }
    }

    return std::tie(info, err);
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

    DownloadFile df;
    std::tie(df, *err) = this->findURLInfo(url);

    int64_t id;

    if (err->isEmpty()) {
        id = df.id;
        if (!df.file.isEmpty())
            r = dir + "\\Files\\" + df.file;
    } else {
        std::tie(id, *err) = saveURLInfos(url, -1, "");
    }

    if (r.isEmpty() && err->isEmpty()) {
        QFuture<DownloadFile> future = QtConcurrent::run(
                &FileLoader::threadPool, this,
                &FileLoader::downloadFileRunnable, id, url);
        QFutureWatcher<DownloadFile>* w =
                new QFutureWatcher<DownloadFile>(this);
        connect(w, SIGNAL(finished()), this,
                SLOT(watcherFileFinished()));
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
        saveURLInfos(r.url, r.size, r.file);
        QString file = r.file;
        if (!file.isEmpty())
            file = dir + "\\Files\\" + file;

        emit this->downloadFileCompleted(r.url, file, r.error);
    }

    w->deleteLater();
}

FileLoader::DownloadFile FileLoader::downloadFileRunnable(int64_t id,
        const QString& url)
{
    FileLoader::DownloadFile r;
    r.url = url;

    QThread::currentThread()->setPriority(QThread::LowestPriority);

    bool b = SetThreadPriority(GetCurrentThread(),
            THREAD_MODE_BACKGROUND_BEGIN);

    CoInitialize(nullptr);

    QString filename = QString::number(id);
    QString fn = dir + "\\Files\\" + filename;
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
            r.size = f.size();
        }
        delete job;
    } else {
        r.error = QObject::tr("Cannot open the file %1").arg(fn);
    }

    CoUninitialize();

    if (b)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);

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
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "ADDRESS TEXT NOT NULL, "
                    "SIZE INTEGER, "
                    "SIZE_MODIFIED INTEGER, "
                    "FILE TEXT)");
            err = SQLUtils::toString(db.lastError());

        }
    }

    if (err.isEmpty()) {
        if (!e) {
            db.exec("CREATE UNIQUE INDEX URL_ADDRESS ON URL("
                    "ADDRESS)");
            err = SQLUtils::toString(db.lastError());
        }
    }
    return err;
}
