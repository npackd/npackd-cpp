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
    explicit SettingsFrame(QWidget *parent = nullptr);
    ~SettingsFrame();

    /**
     * @return repository URLs
     */
    QStringList getRepositoryURLs();

    /**
     * @param urls new repository URL
     * @param comments comments for URLs
     */
    void setRepositoryURLs(const QStringList& urls, const QStringList &comments);

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
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_pushButton_clicked();

    void on_pushButtonProxySettings_clicked();

    void on_pushButtonAddRep_clicked();
    void on_pushButtonRemoveRep_clicked();

private:
    Ui::SettingsFrame *ui;
};

#endif // SETTINGSFRAME_H
