#ifndef LICENSEFORM_H
#define LICENSEFORM_H

#include <QWidget>
#include <QSharedPointer>

#include "license.h"

namespace Ui {
    class LicenseForm;
}


/**
 * @brief form for a license
 */
class LicenseForm : public QWidget {
    Q_OBJECT
public:
    /** the license */
    License* license;

    LicenseForm(QWidget *parent = 0);
    ~LicenseForm();

    /**
     * @param license a license. The object will be managed here.
     */
    void fillForm(License *license);
protected:
    void changeEvent(QEvent *e);

private:
    Ui::LicenseForm *ui;
};

#endif // LICENSEFORM_H
