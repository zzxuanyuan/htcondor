#ifndef DOWNLOAD_REPLICA_TRANSFERER_H
#define DOWNLOAD_REPLICA_TRANSFERER_H

#include "BaseReplicaTransferer.h"

/* Class      : ReplicaDown
 * Description: class, encapsulating the downloading 'condor_transferer'
 *              process behaviour
 */
class DownloadReplicaTransferer : public BaseReplicaTransferer
{
public:
   /* Function  : DownloadReplicaTransferer constructor
    * Arguments : pDaemonSinfulString - uploading daemon sinfull string
    *             pVersionFilePath    - version string in dot-separated format
    *             pStateFilesList     - list of paths to the state files
    */
    DownloadReplicaTransferer(const MyString&  pDaemonSinfulString,
                              const MyString&  pVersionFilePath,
                              const MyString& pStateFilePath):
        BaseReplicaTransferer( pDaemonSinfulString,
                               pVersionFilePath,
                               pStateFilePath ) {};
    /* Function    : initialize
     * Return value: TRANSFERER_TRUE - upon success
     *               TRANSFERER_FALSE - upon failure
     * Description : main function of downloading 'condor_transferer' process,
     *               where the downloading of important files happens
     */
    virtual int initialize();

private:
    int download();
    int downloadFile(MyString& filePath, MyString& extension);

    int transferFileCommand();
};

#endif // DOWNLOAD_REPLICA_TRANSFERER_H
