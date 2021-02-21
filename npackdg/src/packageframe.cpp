#include "packageframe.h"
#include "ui_packageframe.h"

#include "packageversionform.h"
#include "ui_packageversionform.h"

#include <QApplication>
#include <QDesktopServices>
#include <QVariant>
#include <QAction>

#include "package.h"
#include "repository.h"
#include "mainwindow.h"
#include "license.h"
#include "licenseform.h"
#include "packageversionform.h"
#include "dbrepository.h"
#include "uiutils.h"
#include "wpmutils.h"

PackageFrame::PackageFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::PackageFrame)
{
    this->p = new Package("test", "Test");
    ui->setupUi(this);

    MainWindow* mw = MainWindow::getInstance();

    // also update mainwindow.cpp if adding a new action here
    QTableWidget* t = this->ui->tableWidgetVersions;
    t->addAction(mw->findChild<QAction*>("actionInstall"));
    t->addAction(mw->findChild<QAction*>("actionUninstall"));
    t->addAction(mw->findChild<QAction*>("actionUpdate"));
    t->addAction(mw->findChild<QAction*>("actionShow_Details"));
    t->addAction(mw->findChild<QAction*>("actionShow_changelog"));
    t->addAction(mw->findChild<QAction*>("actionRun"));
    t->addAction(mw->findChild<QAction*>("actionOpen_folder"));
    t->addAction(mw->findChild<QAction*>("actionGotoPackageURL"));
    t->addAction(mw->findChild<QAction*>("actionTest_Download_Site"));
    t->addAction(mw->findChild<QAction*>("actionExport"));

    connect(this->ui->listWidgetScreenshots,
            SIGNAL(itemActivated(QListWidgetItem*)), this,
            SLOT(screenshotsItemActivated(QListWidgetItem*)));
}

PackageFrame::~PackageFrame()
{
    qDeleteAll(this->pvs);
    this->pvs.clear();
    delete this->p;
    delete ui;
}

void PackageFrame::reload()
{
    if (this->p) {
        DBRepository* r = DBRepository::getDefault();
        Package* newp = r->findPackage_(p->name);
        if (newp)
            this->fillForm(newp);
    }
}

void PackageFrame::updateIcons(const QString& url)
{
    if (p->getIcon() == url) {
        QIcon icon = MainWindow::getPackageIcon(p->name);
        QPixmap pixmap = icon.pixmap(UIUtils::ICON_SIZE, UIUtils::ICON_SIZE, QIcon::Normal, QIcon::On);
        this->ui->labelIcon->setPixmap(pixmap);
    }

    // screen shots
    MainWindow* mw = MainWindow::getInstance();

    QListWidget* c = this->ui->listWidgetScreenshots;
    std::vector<QString> screenshots = p->getLinks("screenshot");

    bool updated = false;
    for (int i = 0; i < c->count(); i++) {
        QListWidgetItem* item = c->item(i);
        QString url_ = item->data(Qt::UserRole).toString();
        if (url_ == url) {
            QIcon icon = mw->downloadScreenshot(screenshots.at(i));
            item->setIcon(icon);
            updated = true;
        }
    }

    // workaround for a bug(?) in Qt 5.2.1: the space reserved for each
    // screenshot is not updated otherwise
    if (updated)
        c->setFlow(QListView::LeftToRight);
}

void PackageFrame::updateStatus()
{
    for (int i = 0; i < this->ui->tableWidgetVersions->rowCount(); i++) {
        PackageVersion* pv = this->pvs.at(i);
        QTableWidgetItem* item = this->ui->tableWidgetVersions->item(i, 1);
        item->setText(pv->getPath());
    }
}

void PackageFrame::fillForm(Package* p)
{
    delete this->p;
    this->p = p;

    this->ui->lineEditTitle->setText(p->title);
    this->ui->lineEditInternalName->setText(p->name);

    DBRepository* dbr = DBRepository::getDefault();

    QString licenseTitle = QObject::tr("unknown");
    QString err;
    License* lic = dbr->findLicense_(p->license, &err);
    if (!err.isEmpty()) {
        MainWindow::getInstance()->addErrorMessage(err, err, true,
                QMessageBox::Critical);
    }
    if (lic) {
        licenseTitle = "<a href=\"http://www.example.com\">" +
                lic->title.toHtmlEscaped() + "</a>";
        delete lic;
    }
    this->ui->labelLicense->setText(licenseTitle);

    // description, categories, change log
    this->ui->textEditDescription->setText(p->description);

    QString hp;
    if (p->url.isEmpty())
        hp = QObject::tr("unknown");
    else{
        hp = p->url;
        hp = "<a href=\"" + hp.toHtmlEscaped() + "\">" +
                hp.toHtmlEscaped() + "</a>";
    }
    this->ui->labelHomePage->setText(hp);

    this->ui->labelCategory->setText(WPMUtils::join(p->categories, ", "));

    this->ui->labelTags->setText(WPMUtils::join(p->tags, ", "));

    this->ui->labelStars->setText(QString::number(p->stars));

    QString changelog;
    if (p->getChangeLog().isEmpty())
        changelog = QObject::tr("n/a");
    else{
        changelog = p->getChangeLog();
        changelog = "<a href=\"" + changelog.toHtmlEscaped() + "\">" +
                changelog.toHtmlEscaped() + "</a>";
    }
    this->ui->labelChangeLog->setText(changelog);

    QString s;
    if (p->getIssueTracker().isEmpty())
        s = QObject::tr("n/a");
    else{
        s = p->getIssueTracker();
        s = "<a href=\"" + s.toHtmlEscaped() + "\">" +
                s.toHtmlEscaped() + "</a>";
    }
    this->ui->labelIssues->setText(s);

    QIcon icon = MainWindow::getPackageIcon(p->name);
    QPixmap pixmap = icon.pixmap(UIUtils::ICON_SIZE, UIUtils::ICON_SIZE, QIcon::Normal, QIcon::On);
    this->ui->labelIcon->setPixmap(pixmap);

    MainWindow* mw = MainWindow::getInstance();

    QListWidget* c = this->ui->listWidgetScreenshots;
    c->clear();

    std::vector<QString> screenshots = p->getLinks("screenshot");
    int n = screenshots.size();

    // qCDebug(npackd) << n << "screen shots";

    c->setVisible(n > 0);

    for (int i = 0; i < n; i++) {
        QIcon icon = mw->downloadScreenshot(screenshots.at(i));
        if (!icon.isNull()) {
            QListWidgetItem* item = new QListWidgetItem(icon,
                    QString::number(i + 1));
            item->setData(Qt::UserRole,
                    QVariant::fromValue(screenshots.at(i)));
            c->addItem(item);
        }
    }

    QTableWidgetItem *newItem;
    QTableWidget* t = this->ui->tableWidgetVersions;
    t->clear();
    t->setColumnCount(2);
    t->setColumnWidth(1, 400);
    newItem = new QTableWidgetItem(QObject::tr("Version"));
    t->setHorizontalHeaderItem(0, newItem);
    newItem = new QTableWidgetItem(QObject::tr("Installation path"));
    t->setHorizontalHeaderItem(1, newItem);

    qDeleteAll(this->pvs);
    pvs = dbr->getPackageVersions_(p->name, &err);

    if (!err.isEmpty()) {
        MainWindow::getInstance()->addErrorMessage(err,
                QObject::tr("Error fetching package versions: %1").
                arg(err), true, QMessageBox::Critical);
    }

    //qCDebug(npackd) << "PackageFrame::fillForm " << pvs.count() << " " <<
    //        pvs.at(0)->version.getVersionString();

    t->setRowCount(pvs.size());
    for (int i = static_cast<int>(pvs.size()) - 1; i >= 0; i--) {
        PackageVersion* pv = pvs.at(i);

        newItem = new QTableWidgetItem(pv->version.getVersionString());
        t->setItem(i, 0, newItem);

        newItem = new QTableWidgetItem("");
        newItem->setText(pv->getPath());
        t->setItem(i, 1, newItem);
    }
}

void PackageFrame::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void PackageFrame::on_labelLicense_linkActivated(const QString &/*link*/)
{
    MainWindow::getInstance()->openLicense(p->license, true);
}

void PackageFrame::showDetails()
{
    MainWindow* mw = MainWindow::getInstance();
    QList<QTableWidgetItem*> sel =
            this->ui->tableWidgetVersions->selectedItems();
    for (int i = 0; i < sel.count(); i++) {
        QTableWidgetItem* item = sel.at(i);
        if (item->column() == 0) {
            PackageVersion* pv = this->pvs.at(item->row());
            mw->openPackageVersion(pv->package, pv->version, true);
        }
    }
}

std::vector<void*> PackageFrame::getSelected(const QString& type) const
{
    std::vector<void*> res;
    if (type == "Package" && this->p) {
        res.push_back(this->p);
    } else if (type == "PackageVersion") {
        if (this->ui->tableWidgetVersions->hasFocus()) {
            QList<QTableWidgetItem*> sel =
                    this->ui->tableWidgetVersions->selectedItems();
            for (int i = 0; i < sel.count(); i++) {
                QTableWidgetItem* item = sel.at(i);
                if (item->column() == 0) {
                    PackageVersion* pv = this->pvs.at(item->row());
                    res.push_back(pv);
                }
            }
        }
    }
    return res;
}

void PackageFrame::on_tableWidgetVersions_doubleClicked(const QModelIndex &/*index*/)
{
    showDetails();
}

void PackageFrame::on_tableWidgetVersions_itemSelectionChanged()
{
    MainWindow::getInstance()->updateActions();
}

void PackageFrame::screenshotsItemActivated(QListWidgetItem *item)
{
    MainWindow* mw = MainWindow::getInstance();
    QString url = item->data(Qt::UserRole).toString();
    QString err;
    QString filename = mw->fileLoader.downloadFileOrQueue(url, &err);
    if (!err.isEmpty()) {
        QString msg = QObject::tr("Error downloading the file %1: %2").
                arg(url, err);
        mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
    } else if (!filename.isEmpty()) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filename))) {
            QString msg = QObject::tr("Cannot open the file %1").
                    arg(filename);
            mw->addErrorMessage(msg, msg, true, QMessageBox::Critical);
        }
    }
}
