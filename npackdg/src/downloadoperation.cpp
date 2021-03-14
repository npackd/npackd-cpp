#include "downloadoperation.h"

#include "downloader.h"

DownloadOperation::DownloadOperation(const QString &url):
    request(QUrl(url))
{

}

void DownloadOperation::execute(Job *job)
{
    response = Downloader::download(job, request);
}

Operation *DownloadOperation::clone() const
{
    return new DownloadOperation(*this);
}
