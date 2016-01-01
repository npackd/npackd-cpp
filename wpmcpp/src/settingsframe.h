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

    /**
     * @return true = send information about the success of an installation/
     *     removal to https://npackd.appspot.com
     */
    bool getSendInformation();

    /**
     * @param v true = send information about the success of an installation/
     *     removal to https://npackd.appspot.com
     */
    void setSendInformation(bool v);
private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_clicked(QAbstractButton *button);
private:
    Ui::SettingsFrame *ui;
};

#endif // SETTINGSFRAME_H
