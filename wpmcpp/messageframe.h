#ifndef MESSAGEFRAME_H
#define MESSAGEFRAME_H

#include <QFrame>
#include <QMessageBox>

namespace Ui {
    class MessageFrame;
}

class MessageFrame : public QFrame
{
    Q_OBJECT
private:
    QString details;
    int seconds;
    QMessageBox::Icon icon;
public:
    /**
     * @param parent parent widget or 0
     * @param msg one line message
     * @param details multi-line message
     * @param seconds automatically close after this number of seconds or 0 for
     *     "do not close automatically"
     * @param icon message icon
     */
    explicit MessageFrame(QWidget *parent, const QString& msg,
            const QString& details, int seconds, QMessageBox::Icon icon);
    ~MessageFrame();
private slots:
    void timerTimeout();
    void on_pushButtonDetails_clicked();
    void on_pushButtonDismiss_clicked();
private:
    Ui::MessageFrame *ui;
};

#endif // MESSAGEFRAME_H
