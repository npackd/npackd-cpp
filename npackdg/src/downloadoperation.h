#ifndef DOWNLOADOPERATION_H
#define DOWNLOADOPERATION_H

#include <QString>

#include "operation.h"
#include "downloader.h"

/**
 * @brief download a file
 */
class DownloadOperation: public Operation
{
public:
    /**
     * @brief download request
     */
    Downloader::Request request;

    /**
     * @brief response
     */
    Downloader::Response response;

    /**
     * @brief creates an download operation
     * @param url URL to download
     */
    DownloadOperation(const QString& url);

    void execute(Job* job);
    Operation* clone() const;
};

#endif // DOWNLOADOPERATION_H
