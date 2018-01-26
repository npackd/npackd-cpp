#ifndef URLINFO_H
#define URLINFO_H

#include <QObject>

/**
 * @brief information about an URL
 */
class URLInfo : public QObject
{
    Q_OBJECT
public:
    /** URL */
    QString address;

    /** download size or -1 if unknown or -2 if an error occured */
    int64_t size;

    /** date/time when the size was computed */
    time_t sizeModified;

    URLInfo();

    /**
     * @brief -
     * @param address URL
     */
    explicit URLInfo(const QString& address);

    URLInfo(const URLInfo& v);

    URLInfo& operator=(const URLInfo& v);
signals:

public slots:
};

#endif // URLINFO_H
