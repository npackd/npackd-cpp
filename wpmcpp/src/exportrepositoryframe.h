#ifndef EXPORTREPOSITORYFRAME_H
#define EXPORTREPOSITORYFRAME_H

#include <QFrame>

namespace Ui {
class ExportRepositoryFrame;
}

class ExportRepositoryFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ExportRepositoryFrame(QWidget *parent = 0);
    ~ExportRepositoryFrame();

    /**
     * @param txt new text for the list of packages
     */
    void setPackageList(const QString& txt);

    /**
     * @return error message or ""
     */
    QString getError() const;

    /**
     * @return chosen directory
     */
    QString getDirectory() const;
private slots:
    void on_pushButtonDir_clicked();
    void on_lineEditDir_textChanged(const QString &arg1);
private:
    Ui::ExportRepositoryFrame *ui;

    void validate();
};

#endif // EXPORTREPOSITORYFRAME_H
