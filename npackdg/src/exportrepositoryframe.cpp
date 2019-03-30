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

QString ExportRepositoryFrame::getError() const
{
    return ui->labelMessage->text();
}

QString ExportRepositoryFrame::getDirectory() const
{
    return ui->lineEditDir->text();
}

int ExportRepositoryFrame::getExportDefinitions() const
{
    if (ui->radioButtonSuper->isChecked())
        return 0;
    else if (ui->radioButtonDependencies->isChecked())
        return 1;
    else if (ui->radioButtonSuperAndDependencies->isChecked())
        return 2;
    else
        return 3;
}

void ExportRepositoryFrame::on_pushButtonDir_clicked()
{
    QString d = QFileDialog::getExistingDirectory(this,
            QObject::tr("Choose export directory"),
            this->ui->lineEditDir->text());
    if (!d.isEmpty())
        this->ui->lineEditDir->setText(d);
}

void ExportRepositoryFrame::on_lineEditDir_textChanged(const QString &/*arg1*/)
{
    validate();
}

void ExportRepositoryFrame::validate()
{
    QString err;

    if (err.isEmpty()) {
        QDir d(ui->lineEditDir->text());
        if (d.exists() && !d.isEmpty()) {
            err = QObject::tr("Cannot export to an existing non-empty directory");
        }
    }

    ui->labelMessage->setText(err);
}
