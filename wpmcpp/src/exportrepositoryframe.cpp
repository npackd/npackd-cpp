#include <QFileDialog>
#include <QUrl>

#include <shlobj.h>

#include "exportrepositoryframe.h"
#include "ui_exportrepositoryframe.h"
#include "wpmutils.h"

ExportRepositoryFrame::ExportRepositoryFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ExportRepositoryFrame)
{
    ui->setupUi(this);

    ui->lineEditDir->setText(WPMUtils::getShellDir(CSIDL_MYDOCUMENTS));
}

ExportRepositoryFrame::~ExportRepositoryFrame()
{
    delete ui;
}

void ExportRepositoryFrame::setPackageList(const QString &txt)
{
    ui->textEditVersions->setText(txt);
}

QString ExportRepositoryFrame::getError() const
{
    return ui->labelMessage->text();
}

QString ExportRepositoryFrame::getDirectory() const
{
    return ui->lineEditDir->text();
}

void ExportRepositoryFrame::on_pushButtonDir_clicked()
{
    QString d = QFileDialog::getExistingDirectory(this,
            QObject::tr("Choose export directory"),
            this->ui->lineEditDir->text());
    if (!d.isEmpty())
        this->ui->lineEditDir->setText(d);
}

void ExportRepositoryFrame::on_lineEditDir_textChanged(const QString &arg1)
{
    ui->lineEditRepositoryAddress->setText(
                QUrl::fromLocalFile(arg1 + "\\Rep.xml").toString());
    validate();
}

void ExportRepositoryFrame::validate()
{
    QString err;
    QDir d;

    if (err.isEmpty()) {
        if (d.exists(ui->lineEditDir->text())) {
            err = QObject::tr("Cannot export to an existing directory");
        }
    }

    ui->labelMessage->setText(err);
}
