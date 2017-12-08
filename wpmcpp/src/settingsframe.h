#ifndef SETTINGSFRAME_H
#define SETTINGSFRAME_H

#include <QFrame>
#include <QAbstractButton>

#include "wpmutils.h"

namespace Ui {
    class SettingsFrame;
}

class SettingsFrame : public QFrame
{
    Q_OBJECT

public:
    explicit SettingsFrame(QWidget *parent = 0);
    ~SettingsFrame();

    /**
     * @return repository URLs
     */
    QStringList getRepositoryURLs();

    /**
     * @param urls new repository URL
     */
    void setRepositoryURLs(const QStringList& urls);

    /**
     * @return installation directory
     */
    QString getInstallationDirectory();

    /**
     * @param dir installation directory
     */
    void setInstallationDirectory(const QString& dir);

    /**
     * @return chosen value for how to close programs
     */
    DWORD getCloseProcessType();

    /**
     * @param v how to close programs
     */
    void setCloseProcessType(DWORD v);
private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_clicked(QAbstractButton *button);

    void on_pushButtonAddRep_clicked();

	void on_checkBoxSingleInstance_toggled(bool checked);

private:
    Ui::SettingsFrame *ui;
    void addUsedRepository(const QString &repository);
};

#endif // SETTINGSFRAME_H
