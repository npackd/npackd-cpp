#ifndef VIMORGREPAPP_H
#define VIMORGREPAPP_H

#include <QObject>
#include <QString>

#include "packageversion.h"

/**
 * @brief Creates an Npackd repository from the data from vim.org.
 * See also https://bitbucket.org/vimcommunity/vim-pi/src/3477ee1dbdada435910fde6b335be4f55c3d1211/python/vimorg.py?at=master
 */
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
