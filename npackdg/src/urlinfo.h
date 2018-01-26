#ifndef URLINFO_H
#define URLINFO_H

#include <QObject>

class URLInfo : public QObject
{
    Q_OBJECT
public:
    explicit URLInfo(QObject *parent = nullptr);

signals:

public slots:
};

#endif // URLINFO_H