#include "qstringlist.h"

#include "version.h"

const Version Version::EMPTY(-1, -1);

Version::Version()
{
    this->parts = &this->basic[0];
    this->parts[0] = 1;
    this->nparts = 1;
}

Version::Version(const QString &v)
{
    this->parts = &this->basic[0];
    this->parts[0] = 1;
    this->nparts = 1;

    setVersion(v);
}

Version::Version(int a, int b)
{
    this->parts = &basic[0];
    this->parts[0] = a;
    this->parts[1] = b;
    this->nparts = 2;
}

Version::Version(const Version &v)
{
    if (v.nparts <= BASIC_PARTS)
        this->parts = &basic[0];
    else
        this->parts = new int[v.nparts];
    this->nparts = v.nparts;
    memcpy(parts, v.parts, sizeof(parts[0]) * nparts);
}

Version& Version::operator =(const Version& v)
{
    if (this != &v) {
        if (this->parts != &basic[0])
            delete[] this->parts;
        if (v.nparts <= BASIC_PARTS)
            this->parts = &basic[0];
        else
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

bool Version::operator <=(const Version& v) const
{
    return this->compare(v) <= 0;
}

bool Version::operator >(const Version& v) const
{
    return this->compare(v) > 0;
}

Version::~Version()
{
    if (this->parts != this->basic)
        delete[] this->parts;
}

void Version::setVersion(int a, int b)
{
    if (this->parts != this->basic)
        delete[] this->parts;
    this->parts = basic;
    this->parts[0] = a;
    this->parts[1] = b;
    this->nparts = 2;
}

void Version::setVersion(int a, int b, int c)
{
    if (this->parts != this->basic)
        delete[] this->parts;
    this->parts = basic;
    this->parts[0] = a;
    this->parts[1] = b;
    this->parts[2] = c;
    this->nparts = 3;
}

void Version::setVersion(int a, int b, int c, int d)
{
    if (this->parts != this->basic)
        delete[] this->parts;
    this->parts = basic;
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
                if (this->parts != basic)
                    delete[] this->parts;
                this->nparts = sl.count();
                if (nparts <= BASIC_PARTS)
                    this->parts = basic;
                else
                    this->parts = new int[nparts];
                for (int i = 0; i < nparts; i++) {
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
    int* newParts;
    if (nparts + 1 <= BASIC_PARTS)
        newParts = basic;
    else
        newParts = new int[nparts + 1];

    newParts[0] = number;
    memmove(newParts + 1, this->parts, sizeof(parts[0]) * (this->nparts));
    if (this->parts != basic)
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
        int* newParts;
        if (this->nparts - n <= BASIC_PARTS)
            newParts = basic;
        else
            newParts = new int[this->nparts - n];
        memmove(newParts, this->parts, sizeof(parts[0]) * (this->nparts - n));
        if (this->parts != basic)
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
