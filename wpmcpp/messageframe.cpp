#include "messageframe.h"
#include "ui_messageframe.h"

#include <windows.h>

#include <QApplication>
#include <QTimer>

#include "mainwindow.h"

MessageFrame::MessageFrame(QWidget *parent, const QString& msg,
        const QString& details, int seconds, QMessageBox::Icon icon) :
    QFrame(parent),
    ui(new Ui::MessageFrame)
{
    ui->setupUi(this);

    if (seconds == 0)
        this->ui->pushButtonDismiss->setText(QObject::tr("Dismiss"));
    else
        this->ui->pushButtonDismiss->setText(
                QString(QObject::tr("Dismiss (%1 seconds)")).
                arg(seconds));
    this->seconds = seconds;
    this->ui->label->setText(msg);
    this->details = details;
    this->ui->pushButtonDetails->setEnabled(!details.isEmpty());
    this->icon = icon;

    if (seconds > 0) {
        QTimer* t = new QTimer(this);
        connect(t, SIGNAL(timeout()), this, SLOT(timerTimeout()));
        t->start(10000);
    }
}


MessageFrame::~MessageFrame()
{
    delete ui;
}

void MessageFrame::timerTimeout()
{
    bool active = this->window()->isActiveWindow();

    if (active) {
        LASTINPUTINFO LII;
        memset(&LII, 0, sizeof(LII));
        LII.cbSize = sizeof(LII);
        if (GetLastInputInfo(&LII)) {
            DWORD now = GetTickCount();
            if (now - LII.dwTime > 10000) {
                active = false;
            }
        }
    }

    this->ui->pushButtonDismiss->setText(
            QString(QObject::tr("Dismiss (%1 seconds)")).arg(seconds));
    if (active) {
        this->seconds -= 10;
        if (this->seconds <= 0) {
            this->deleteLater();
        }
    }
}

void MessageFrame::on_pushButtonDetails_clicked()
{
    QString title = this->ui->label->text();
    if (title.length() > 20)
        title = title.left(20) + "...";
    MainWindow::getInstance()->addTextTab(title,
            this->details);
    this->deleteLater();
}

void MessageFrame::on_pushButtonDismiss_clicked()
{
    this->deleteLater();
}
