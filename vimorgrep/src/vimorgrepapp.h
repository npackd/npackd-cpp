#ifndef VIMORGREPAPP_H
#define VIMORGREPAPP_H

#include <QObject>

class VimOrgRepApp: public QObject
{
    Q_OBJECT
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
