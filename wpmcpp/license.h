#ifndef LICENSE_H
#define LICENSE_H

#include "qstring.h"

/**
 * License description.
 */
class License
{
public:
    /** full qualified ID like "org.gnu.GPLv3" */
    QString name;

    /** license title like "GPLv3" */
    QString title;

    /** multiline description of the license */
    QString description;

    /**
     * URL to a http:// or https:// HTML page describing the license or ""
     */
    QString url;

    /**
     * @param name ID of this license
     * @param title title of this license
     */
    License(QString name, QString title);

    /**
     * @brief creates a copy of this object
     * @return [ownership:caller] copy
     */
    License* clone() const;
};

#endif // LICENSE_H
