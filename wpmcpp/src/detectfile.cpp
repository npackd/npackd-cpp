#include "detectfile.h"

DetectFile::DetectFile()
{
}

DetectFile *DetectFile::clone() const
{
    DetectFile* r = new DetectFile();
    r->path = this->path;
    r->sha1 = this->sha1;
    return r;
}
