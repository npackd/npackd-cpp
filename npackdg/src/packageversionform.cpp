#include "packageversionform.h"
#include "ui_packageversionform.h"

#include <QApplication>
#include <QDesktopServices>
#include <QSharedPointer>

#include "package.h"
#include "repository.h"
#include "mainwindow.h"
#include "license.h"
#include "licenseform.h"
#include "dbrepository.h"
#include "installedpackages.h"
#include "installedpackageversion.h"

PackageVersionForm::PackageVersionForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PackageVersionForm)
{
    this->pv = new PackageVersion();
    ui->setupUi(this);
}

void PackageVersionForm::updateIcons()
{
    QIcon icon = MainWindow::getPackageIcon(this->pv->package);
    QPixmap pixmap = icon.pixmap(32, 32, QIcon::Normal, QIcon::On);
    this->ui->labelIcon->setPixmap(pixmap);
}

QList<void*> PackageVersionForm::getSelected(const QString& type) const
{
    QList<void*> res;
    if (type == "PackageVersion" && this->pv) {
        res.append(this->pv);
    }
    return res;
}

void PackageVersionForm::updateStatus()
{
    DBRepository* dbr = DBRepository::getDefault();

    this->ui->lineEditStatus->setText(pv->getStatus(dbr));
    this->ui->lineEditPath->setText(pv->getPath());

    InstalledPackages* ip = InstalledPackages::getDefault();
    InstalledPackageVersion* ipv = ip->find(pv->package, pv->version);
    if (ipv) {
        this->ui->lineEditDetectionInfo->setText(ipv->detectionInfo);
    } else {
        this->ui->lineEditDetectionInfo->setText("");
    }
    delete ipv;
}

void PackageVersionForm::reload()
{
    DBRepository* r = DBRepository::getDefault();
    QString err;
    PackageVersion* newpv = r->findPackageVersion_(
            this->pv->package, this->pv->version, &err);
    if (err.isEmpty() && newpv)
        this->fillForm(newpv);
}

void PackageVersionForm::fillForm(PackageVersion* pv)
{
    delete this->pv;
    this->pv = pv;

    // qCDebug(npackd) << pv.data()->toString();

    DBRepository* r = DBRepository::getDefault();

    this->ui->lineEditTitle->setText(pv->getPackageTitle());
    Version v = pv->version;
    v.normalize();
    this->ui->lineEditVersion->setText(v.getVersionString());
    this->ui->lineEditInternalName->setText(pv->package);

    Package* p = r->findPackage_(pv->package);

    QString licenseTitle = QObject::tr("unknown");
    if (p) {
        QString err;
        License* lic = r->findLicense_(p->license, &err);
        if (!err.isEmpty())
            MainWindow::getInstance()->addErrorMessage(err, err, true,
                    QMessageBox::Critical);
        if (lic) {
            licenseTitle = "<a href=\"http://www.example.com\">" +
                    lic->title.toHtmlEscaped() + "</a>";
            delete lic;
        }
    }
    this->ui->labelLicense->setText(licenseTitle);

    if (p) {
        this->ui->textEditDescription->setText(p->description);
    }

    updateStatus();

    QString dl;
    if (!pv->download.isValid())
        dl = QObject::tr("n/a");
    else {
        dl = pv->download.toString();        
        dl = "<a href=\"" + dl.toHtmlEscaped() + "\">" +
                dl.toHtmlEscaped() + "</a>";
    }
    this->ui->labelDownloadURL->setText(dl);

    QString sha1;
    if (pv->sha1.isEmpty())
        sha1 = QObject::tr("n/a");
    else {
        sha1 = pv->sha1;
        if (pv->hashSumType == QCryptographicHash::Sha1)
            sha1.prepend("SHA-1: ");
        else
            sha1.prepend("SHA-256: ");
    }
    this->ui->lineEditSHA1->setText(sha1);

    QString type;
    if (pv->type == PackageVersion::Type::ONE_FILE)
        type = "one-file";
    else if (pv->type == PackageVersion::Type::INNO_SETUP)
        type = "inno-setup";
    else if (pv->type == PackageVersion::Type::NSIS)
        type = "nsis";
    else
        type = "zip";
    this->ui->lineEditType->setText(type);

    QString details;
    for (int i = 0; i < pv->importantFiles.count(); i++) {
        details.append(pv->importantFilesTitles.at(i));
        details.append(" (");
        details.append(pv->importantFiles.at(i));
        details.append(")\r\n");
    }
    this->ui->textEditImportantFiles->setText(details);

    details = "";
    for (int i = 0; i < pv->cmdFiles.count(); i++) {
        details.append(pv->cmdFiles.at(i));
        details.append("\r\n");
    }
    this->ui->textEditCmdFiles->setText(details);

    QLayoutItem *child;
    while ((child = this->ui->frameDependencies->layout()->takeAt(0)) != nullptr) {
        delete child;
    }
    for (int i = 0; i < pv->dependencies.count(); i++) {
        Dependency* d = pv->dependencies.at(i);

        QString txt = "<a href=\"" + QString::number(i) + "\">" +
                r->toString(*d, false) + "</a> ";

        /*
        InstalledPackageVersion* ipv = d->findHighestInstalledMatch();
        if (ipv) {
            txt += "resolved to " + ipv->version.getVersionString();
            delete ipv;
        } else {
            QList<PackageVersion*> avoid;
            PackageVersion* pv = d->findBestMatchToInstall(avoid, &err);
            if (pv) {
                txt += "resolved to " + pv->version.getVersionString();
                delete pv;
            } else {
                txt += QObject::tr("This dependency is not available");
            }
        }
        */

        QLabel* label = new QLabel(txt);
        label->setMouseTracking(true);
        label->setFocusPolicy(Qt::StrongFocus);
        label->setToolTip(QObject::tr("Show the package for this dependency"));
        label->setTextInteractionFlags(Qt::TextSelectableByMouse |
                Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse |
                Qt::LinksAccessibleByKeyboard);
        connect(label, SIGNAL(linkActivated(QString)), this,
                SLOT(dependencyLinkActivated(QString)));

        this->ui->frameDependencies->layout()->addWidget(label);
    }

    updateIcons();

    this->ui->tabWidgetTextFiles->clear();
    for (int i = 0; i < pv->files.count(); i++) {
        QTextEdit* w = new QTextEdit(this->ui->tabWidgetTextFiles);
        w->setText(pv->files.at(i)->content);
        w->setReadOnly(true);
        this->ui->tabWidgetTextFiles->addTab(w, pv->files.at(i)->path);
    }

    delete p;
}

PackageVersionForm::~PackageVersionForm()
{
    delete pv;
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

void PackageVersionForm::on_labelLicense_linkActivated(QString /*link*/)
{
    DBRepository* r = DBRepository::getDefault();
    Package* p = r->findPackage_(pv->package);

    License* lic = nullptr;
    QString err;
    if (p) {
        lic = r->findLicense_(p->license, &err);
        if (!err.isEmpty())
            MainWindow::getInstance()->addErrorMessage(err, err, true,
                    QMessageBox::Critical);
    }
    if (lic)
        MainWindow::getInstance()->openLicense(p->license, true);

    delete lic;
    delete p;
}

void PackageVersionForm::dependencyLinkActivated(const QString &link)
{
    QString err;

    bool ok;
    int index = link.toInt(&ok);
    if (ok && index < this->pv->dependencies.count()) {
        Dependency* d = pv->dependencies.at(index);
        MainWindow::getInstance()->openPackage(d->package, true);
    } else {
        err = QObject::tr("Invalid dependency link");
    }

    if (!err.isEmpty())
        MainWindow::getInstance()->addErrorMessage(err, err, true,
                QMessageBox::Critical);
}

