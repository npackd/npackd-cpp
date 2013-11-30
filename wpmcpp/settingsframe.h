#ifndef SETTINGSFRAME_H
#define SETTINGSFRAME_H

#include <QFrame>
#include <QAbstractButton>

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
private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_clicked(QAbstractButton *button);

private:
    Ui::SettingsFrame *ui;
};

#endif // SETTINGSFRAME_H
