#ifndef PACKAGEVERSIONFORM_H
#define PACKAGEVERSIONFORM_H

#include <QWidget>
#include <QSharedPointer>

#include "packageversion.h"
#include "selection.h"

namespace Ui {
    class PackageVersionForm;
}

class PackageVersionForm : public QWidget, public Selection {
    Q_OBJECT
public:
    /** PackageVersion associated with this form or 0 */
    PackageVersion* pv;

    PackageVersionForm(QWidget *parent = 0);
    ~PackageVersionForm();

    /**
     * Fills the form with the data of a package version.
     *
     * @param pv package version. The object will be destroyed later here.
     */
    void fillForm(PackageVersion *pv);

    /**
     * Updates the view if a new icon was downloaded.
     */
    void updateIcons();

    /**
     * Updates package status.
     */
    void updateStatus();

    QList<void*> getSelected(const QString& type) const;

    /**
     * @brief re-read the package version information from the database
     */
    void reload();
protected:
    void changeEvent(QEvent *e);

private:
    Ui::PackageVersionForm *ui;

private slots:
    void on_labelLicense_linkActivated(QString link);
};

#endif // PACKAGEVERSIONFORM_H
