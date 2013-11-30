#include "packageframe.h"
#include "ui_packageframe.h"

#include "packageversionform.h"
#include "ui_packageversionform.h"

#include <QApplication>
#include <QDesktopServices>
#include <QSharedPointer>
#include <QDebug>

#include "package.h"
#include "repository.h"
#include "mainwindow.h"
#include "license.h"
#include "licenseform.h"
#include "packageversionform.h"
#include "dbrepository.h"

PackageFrame::PackageFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::PackageFrame)
{
    this->p = 0;
    ui->setupUi(this);

    MainWindow* mw = MainWindow::getInstance();

    QTableWidget* t = this->ui->tableWidgetVersions;
    t->addAction(mw->findChild<QAction*>("actionInstall"));
    t->addAction(mw->findChild<QAction*>("actionUninstall"));
    t->addAction(mw->findChild<QAction*>("actionUpdate"));
    t->addAction(mw->findChild<QAction*>("actionShow_Details"));
    t->addAction(mw->findChild<QAction*>("actionTest_Download_Site"));
}

PackageFrame::~PackageFrame()
{
    qDeleteAll(this->pvs);
    this->pvs.clear();
    delete this->p;
    delete ui;
}

void PackageFrame::updateIcons()
{
    QIcon icon = MainWindow::getPackageVersionIcon(p->name);
    QPixmap pixmap = icon.pixmap(32, 32, QIcon::Normal, QIcon::On);
    this->ui->labelIcon->setPixmap(pixmap);
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
    if (p) {
        QString err;
        License* lic = dbr->findLicense_(p->license, &err);
        if (!err.isEmpty()) {
            MainWindow::getInstance()->addErrorMessage(err, err, true,
                    QMessageBox::Critical);
        }
        if (lic) {
            licenseTitle = "<a href=\"http://www.example.com\">" +
                    Qt::escape(lic->title) + "</a>";
            delete lic;
        }
    }
    this->ui->labelLicense->setText(licenseTitle);

    if (p) {
        this->ui->textEditDescription->setText(p->description);

        QString hp;
        if (p->url.isEmpty())
            hp = QObject::tr("unknown");
        else{
            hp = p->url;
            hp = "<a href=\"" + Qt::escape(hp) + "\">" + Qt::escape(hp) +
                    "</a>";
        }
        this->ui->labelHomePage->setText(hp);
        this->ui->labelCategory->setText(p->categories.join(", "));
    }

    updateIcons();

    QTableWidgetItem *newItem;
    QTableWidget* t = this->ui->tableWidgetVersions;
    t->clear();
    t->setColumnCount(2);
    t->setColumnWidth(1, 400);
    newItem = new QTableWidgetItem(QObject::tr("Version"));
    t->setHorizontalHeaderItem(0, newItem);
    newItem = new QTableWidgetItem(QObject::tr("Installation path"));
    t->setHorizontalHeaderItem(1, newItem);

    QString err;
    qDeleteAll(this->pvs);
    pvs = dbr->getPackageVersions_(p->name, &err);

    if (!err.isEmpty()) {
        MainWindow::getInstance()->addErrorMessage(err,
                QObject::tr("Error fetching package versions: %1").
                arg(err), true, QMessageBox::Critical);
    }

    //qDebug() << "PackageFrame::fillForm " << pvs.count() << " " <<
    //        pvs.at(0)->version.getVersionString();

    t->setRowCount(pvs.size());
    for (int i = pvs.count() - 1; i >= 0; i--) {
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

void PackageFrame::on_labelLicense_linkActivated(const QString &link)
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

QList<void*> PackageFrame::getSelected(const QString& type) const
{
    QList<void*> res;
    if (type == "Package" && this->p) {
        res.append(this->p);
    } else if (type == "PackageVersion") {
        if (this->ui->tableWidgetVersions->hasFocus()) {
            QList<QTableWidgetItem*> sel =
                    this->ui->tableWidgetVersions->selectedItems();
            for (int i = 0; i < sel.count(); i++) {
                QTableWidgetItem* item = sel.at(i);
                if (item->column() == 0) {
                    PackageVersion* pv = this->pvs.at(item->row());
                    res.append(pv);
                }
            }
        }
    }
    return res;
}

void PackageFrame::on_tableWidgetVersions_doubleClicked(const QModelIndex &index)
{
    showDetails();
}

void PackageFrame::on_tableWidgetVersions_itemSelectionChanged()
{
    MainWindow::getInstance()->updateActions();
}
