// ReplicaVersion.h: interface for the ReplicaVersion class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(HAD_ReplicaVersion_H__)
#define HAD_ReplicaVersion_H__

#include <time.h>

class ReplicaVersion {

public:
    /*
        Constr'rs
    */

    ReplicaVersion();
    ReplicaVersion(time_t t1,time_t t2,time_t t3,time_t t_update,long size);


    //     Const'r.Version is in format "t1.t2.t3.t_update.size";
    ReplicaVersion(char* version);

    //    Const'r. filename is name of the file that contains version information
    ReplicaVersion(char* filename,bool flag);

    ReplicaVersion(const ReplicaVersion& v){ }

    ~ReplicaVersion();

    /*
      Set T(updated) - called each replication timeout if negotiator file size
                       was changed
    */
    void setLastSizeUpdateTime(time_t t_update);

    void setLastLeaderStartTime(time_t t3);
    void setFileName(char* file);

    //    Set negotiator file size
    void setSize(long size);


    //    Get negotiator file size
    long getSize();
    time_t getTime1(){return time1;};
    time_t getTime2(){return time2;};
    time_t getTime3(){return time3;};
    time_t getTimeLastSizeUpdate(){return timeLastSizeUpdate;};


    /*
        Read version information from the version file
    */
    bool fetch();

    /*
        Write version information to the version file
    */
    void saveVersion();


    /*
        Comparison operators
    */


    bool operator<(ReplicaVersion& ver);
    bool operator>(ReplicaVersion& ver);
    bool operator==(ReplicaVersion& ver);
    bool operator<=(ReplicaVersion& ver);
    bool operator>=(ReplicaVersion& ver);

    void operator=(ReplicaVersion& ver);

    // don't forget to free returned values
    char* versionToSendString();
    char* versionToPrintString();

private:
    time_t time1;
    time_t time2;
    time_t time3;
    time_t timeLastSizeUpdate;

    long size;
    char* filename;

};


#endif
