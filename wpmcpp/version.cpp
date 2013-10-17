#include "qstringlist.h"

#include "version.h"

const Version Version::EMPTY(-1, -1);

Version::Version()
{
    this->parts = new int[1];
    this->parts[0] = 1;
    this->nparts = 1;
}

Version::Version(const QString &v)
{
    this->parts = new int[1];
    this->parts[0] = 1;
    this->nparts = 1;

    setVersion(v);
}

Version::Version(int a, int b)
{
    this->parts = new int[2];
    this->parts[0] = a;
    this->parts[1] = b;
    this->nparts = 2;
}

Version::Version(const Version &v)
{
    this->parts = new int[v.nparts];
    this->nparts = v.nparts;
    memcpy(parts, v.parts, sizeof(parts[0]) * nparts);
}

Version& Version::operator =(const Version& v)
{
    if (this != &v) {
        delete[] this->parts;
        this->parts = new int[v.nparts];
        this->nparts = v.nparts;
        memcpy(parts, v.parts, sizeof(parts[0]) * nparts);
    }
    return *this;
}

bool Version::operator !=(const Version& v) const
{
    return this->compare(v) != 0;
}

bool Version::operator ==(const Version& v) const
{
    return this->compare(v) == 0;
}

bool Version::operator <(const Version& v) const
{
    return this->compare(v) < 0;
}

bool Version::operator >(const Version& v) const
{
    return this->compare(v) > 0;
}

Version::~Version()
{
    delete[] this->parts;
}

void Version::setVersion(int a, int b)
{
    delete[] this->parts;
    this->parts = new int[2];
    this->parts[0] = a;
    this->parts[1] = b;
    this->nparts = 2;
}

void Version::setVersion(int a, int b, int c)
{
    delete[] this->parts;
    this->parts = new int[3];
    this->parts[0] = a;
    this->parts[1] = b;
    this->parts[2] = c;
    this->nparts = 3;
}

void Version::setVersion(int a, int b, int c, int d)
{
    delete[] this->parts;
    this->parts = new int[4];
    this->parts[0] = a;
    this->parts[1] = b;
    this->parts[2] = c;
    this->parts[3] = d;
    this->nparts = 4;
}

bool Version::setVersion(const QString& v)
{
    bool result = false;
    if (!v.trimmed().isEmpty()) {
        QStringList sl = v.split(".", QString::KeepEmptyParts);

        if (sl.count() > 0) {
            bool ok = true;
            for (int i = 0; i < sl.count(); i++) {
                sl.at(i).toInt(&ok);
                if (!ok)
                    break;
            }

            if (ok) {
                delete[] this->parts;
                this->parts = new int[sl.count()];
                this->nparts = sl.count();
                for (int i = 0; i < sl.count(); i++) {
                    this->parts[i] = sl.at(i).toInt();
                }
                result = true;
            }
        }
    }
    return result;
}

void Version::prepend(int number)
{
    int* newParts = new int[this->nparts + 1];
    newParts[0] = number;
    memmove(newParts + 1, this->parts, sizeof(int) *
            (this->nparts));
    delete[] this->parts;
    this->parts = newParts;
    this->nparts = this->nparts + 1;
}

QString Version::getVersionString(int nparts) const
{
    QString r;
    for (int i = 0; i < nparts; i++) {
        if (i != 0)
            r.append(".");
        if (i >= this->nparts)
            r.append('0');
        else
            r.append(QString::number(this->parts[i]));
    }
    return r;
}

QString Version::getVersionString() const
{
    QString r;
    for (int i = 0; i < this->nparts; i++) {
        if (i != 0)
            r.append('.');
        r.append(QString::number(this->parts[i]));
    }
    return r;
}

int Version::getNParts() const
{
    return this->nparts;
}

void Version::normalize()
{
    int n = 0;
    for (int i = this->nparts - 1; i > 0; i--) {
        if (this->parts[i] == 0)
            n++;
        else
            break;
    }

    if (n > 0) {
        int* newParts = new int[this->nparts - n];
        memmove(newParts, this->parts, sizeof(int) *
                (this->nparts - n));
        delete[] this->parts;
        this->parts = newParts;
        this->nparts = this->nparts - n;
    }
}

bool Version::isNormalized() const
{
    return this->parts[this->nparts - 1] != 0;
}


int Version::compare(const Version &other) const
{
    int nmax = nparts;
    if (other.nparts > nmax)
        nmax = other.nparts;

    int r = 0;
    for (int i = 0; i < nmax; i++) {
        int a;
        if (i < this->nparts)
            a = this->parts[i];
        else
            a = 0;
        int b;
        if (i < other.nparts)
            b = other.parts[i];
        else
            b = 0;

        if (a < b) {
            r = -1;
            break;
        } else if (a > b) {
            r = 1;
            break;
        }
    }
    return r;
}
