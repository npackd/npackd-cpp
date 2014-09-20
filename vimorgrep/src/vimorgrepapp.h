#ifndef VIMORGREPAPP_H
#define VIMORGREPAPP_H

#include <QObject>
#include <QString>

#include "packageversion.h"

class VimOrgRepApp: public QObject
{
    Q_OBJECT
    PackageVersion *createZIPPackage(const QString &packageName, const QString &scriptVersion, const QString &jpackage, const QString &srcId,
                                     const QString &vimVersion);
    PackageVersion *createTarGZPackage(const QString &packageName, const QString &scriptVersion, const QString &jpackage, const QString &srcId, const QString &vimVersion);
    PackageVersion *createVimSyntaxPackage(const QString &packageName,
            const QString &scriptVersion, const QString &jpackage,
            const QString &srcId, const QString &vimVersion,
            const QString& scriptType);
public:
    VimOrgRepApp();
public slots:
    /**
     * Process the command line.
     *
     * @return exit code
     */
    int process();
};

#endif // VIMORGREPAPP_H
