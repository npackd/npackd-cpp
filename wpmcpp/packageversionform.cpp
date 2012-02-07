#include <xapian.h>

#include "packageversionform.h"
#include "ui_packageversionform.h"

#include "qdesktopservices.h"

#include "package.h"
#include "repository.h"
#include "mainwindow.h"
#include "license.h"
#include "licenseform.h"

PackageVersionForm::PackageVersionForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PackageVersionForm)
{
    ui->setupUi(this);
    this->pv = 0;
}

void PackageVersionForm::updateIcons()
{
    Repository* rep = Repository::getDefault();

    Package* p = rep->findPackage(this->pv->package_);
    QIcon icon = MainWindow::getPackageIcon(p);
    QPixmap pixmap = icon.pixmap(32, 32, QIcon::Normal, QIcon::On);
    this->ui->labelIcon->setPixmap(pixmap);
}

void PackageVersionForm::updateStatus()
{
    Repository* rep = Repository::getDefault();
    InstalledPackageVersion* ipv = rep->findInstalledPackageVersion(pv);
    this->ui->lineEditStatus->setText(pv->getStatus());
    this->ui->lineEditPath->setText(ipv ? ipv->ipath : "");
}

void PackageVersionForm::fillForm(const QString& package, const Version& version)
{
    Repository* r = Repository::getDefault();

    delete this->pv;
    this->pv = r->findPackageVersion(package, version);

    this->ui->lineEditTitle->setText(pv->getPackageTitle());
    this->ui->lineEditVersion->setText(pv->version.getVersionString());
    this->ui->lineEditInternalName->setText(pv->getPackage());

    Package* p = r->findPackage(pv->getPackage());

    QString licenseTitle = "unknown";
    if (p) {
        License* lic = r->findLicense(p->license);
        if (lic) {
            licenseTitle = "<a href=\"http://www.example.com\">" +
                    Qt::escape(lic->title) + "</a>";
        }
    }
    this->ui->labelLicense->setText(licenseTitle);

    if (p) {
        this->ui->textEditDescription->setText(p->description);

        QString hp;
        if (p->url.isEmpty())
            hp = "unknown";
        else{
            hp = p->url;
            hp = "<a href=\"" + Qt::escape(hp) + "\">" + Qt::escape(hp) +
                    "</a>";
        }
        this->ui->labelHomePage->setText(hp);
    }

    updateStatus();

    QString dl;
    if (!pv->download.isValid())
        dl = "n/a";
    else {
        dl = pv->download.toString();        
        dl = "<a href=\"" + Qt::escape(dl) + "\">" + Qt::escape(dl) + "</a>";
    }
    this->ui->labelDownloadURL->setText(dl);

    QString sha1;
    if (pv->sha1.isEmpty())
        sha1 = "n/a";
    else
        sha1 = pv->sha1;
    this->ui->lineEditSHA1->setText(sha1);

    this->ui->lineEditType->setText(pv->type == 0 ? "zip" : "one-file");

    QString details;
    for (int i = 0; i < pv->importantFiles.count(); i++) {
        details.append(pv->importantFilesTitles.at(i));
        details.append(" (");
        details.append(pv->importantFiles.at(i));
        details.append(")\n");
    }
    this->ui->textEditImportantFiles->setText(details);

    details = "";
    for (int i = 0; i < pv->dependencies.count(); i++) {
        Dependency* d = pv->dependencies.at(i);
        details.append(d->toString());
        details.append("\n");
    }
    this->ui->textEditDependencies->setText(details);

    updateIcons();
}

PackageVersionForm::~PackageVersionForm()
{
    delete this->pv;
    delete ui;
}

void PackageVersionForm::changeEvent(QEvent *e)
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

void PackageVersionForm::on_labelLicense_linkActivated(QString link)
{
    QTabWidget* tabWidget = dynamic_cast<QTabWidget*>(
            this->parentWidget()->parentWidget());
    if (!tabWidget)
        return;

    LicenseForm* f = new LicenseForm(tabWidget);

    Repository* r = Repository::getDefault();
    Package* p = r->findPackage(pv->getPackage());

    License* lic = 0;
    if (p)
        lic = r->findLicense(p->license);
    if (!lic)
        return;

    f->fillForm(lic);
    tabWidget->addTab(f, lic->title);
    tabWidget->setCurrentIndex(tabWidget->count() - 1);
}

