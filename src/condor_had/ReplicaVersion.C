// ReplicaVersion.C: implementation for the ReplicaVersion class.
//
//////////////////////////////////////////////////////////////////////

#include <fstream.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ReplicaVersion.h"


ReplicaVersion::ReplicaVersion(){
    time1 = -1;
    time2 = -1;
    time3 = -1;
    timeLastSizeUpdate = -1;

    size = 0;
    filename = NULL;
}

ReplicaVersion::ReplicaVersion(time_t t1,time_t t2,time_t t3,time_t t_update,long size_){
    time1 = t1;
    time2 = t2;
    time3 = t3;
    timeLastSizeUpdate = t_update;

    size = size_;
    filename = NULL;
}

ReplicaVersion::ReplicaVersion(char* version){
    char* str = (char*)malloc(strlen(version)+1);
    strcpy(str,version);
    char* tmp1;
    long time1_tmp,time2_tmp,time3_tmp,timeLastSizeUpdate_tmp,size_tmp;

    // read time1
    tmp1 = strtok(str,".");
    if(tmp1 == NULL){
        time1_tmp = -1;
    }else{
        time1_tmp = atol(tmp1);
    }

    // read time2
    tmp1 = strtok(NULL,".");
    if(tmp1 == NULL){
        time2_tmp = -1;
    }else{
        time2_tmp = atol(tmp1);
    }

    // read time3
    tmp1 = strtok(NULL,".");
    if(tmp1 == NULL){
        time3_tmp = -1;
    }else{
        time3_tmp = atol(tmp1);
    }

    // read timeLastSizeUpdate
    tmp1 = strtok(NULL,".");
    if(tmp1 == NULL){
        timeLastSizeUpdate_tmp = -1;
    }else{
        timeLastSizeUpdate_tmp = atol(tmp1);
    }

    // read timeLastSizeUpdate
    tmp1 = strtok(NULL,".");
    if(tmp1 == NULL){
        size_tmp = -1;
    }else{
        size_tmp = atol(tmp1);
    }

    time1 = time1_tmp;
    time2 = time2_tmp;
    time3 = time3_tmp;
    timeLastSizeUpdate = timeLastSizeUpdate_tmp;
    size = size_tmp;

}

ReplicaVersion::ReplicaVersion(char* name,bool flag){
#if 0 // debug
    time1 = -1;
    time2 = -1;
    time3 = -1;
    timeLastSizeUpdate = -1;

    size = 0;
    filename = (char*)malloc(strlen(name) + 1);
    strcpy(filename,name);

    if(!fetch()){
             saveVersion();   // no such file - create new one                  
    }else{
            //dprintf(D_ALWAYS,"Cannot open file %s for writing \n",filename);
    }

#endif
}

ReplicaVersion::~ReplicaVersion(){
    if(filename != NULL){
        free( filename );
    }
}

void ReplicaVersion::setLastSizeUpdateTime(time_t t_update){
    timeLastSizeUpdate =  t_update;
}

void ReplicaVersion::setLastLeaderStartTime(time_t t3){
    time1 = time2;
    time2 = time3;
    time3 = t3;
    saveVersion();
}

void ReplicaVersion::setFileName(char* filename_){
    if(filename != NULL){
        free( filename );
    }
    filename = (char*)malloc(strlen(filename_) + 1);
    strcpy(filename,filename_);
}

void ReplicaVersion::setSize(long size_){
    size = size_;
}

long ReplicaVersion::getSize(){
    return size;
}

bool ReplicaVersion::fetch(){
    char str[200];

    ifstream file_op(filename);

    // read time1
    if(file_op.eof())
        return false;

    file_op.getline(str,200);
    long time1_tmp = atol(str);      // time_t is generally defined by default to long.

    // read time2
    if(file_op.eof())
        return false;

    file_op.getline(str,200);
    long time2_tmp = atol(str);

    // read time3
    if(file_op.eof())
        return false;

    file_op.getline(str,200);
    long time3_tmp = atol(str);


    // read timeLastSizeUpdate
    if(file_op.eof())
        return false;

    file_op.getline(str,200);
    long timeLastSizeUpdate_tmp = atol(str);

    // read size
    if(file_op.eof())
        return false;

    file_op.getline(str,200);
    long size_tmp = atol(str);

    file_op.close();

    time1 = time1_tmp;
    time2 = time2_tmp;
    time3 = time3_tmp;
    timeLastSizeUpdate = timeLastSizeUpdate_tmp;
    size = size_tmp;
    return true;

}
void ReplicaVersion::saveVersion(){
    ofstream file_op(filename);
    file_op << time1 << "\n";
    file_op << time2 << "\n";
    file_op << time3 << "\n";
    file_op << timeLastSizeUpdate << "\n";
    file_op << size << "\n";
    file_op.close();
}

bool ReplicaVersion::operator<(ReplicaVersion& ver){
    if(time1 == ver.getTime1() &&
      time2 == ver.getTime2() &&
      time3 == ver.getTime3()){
        return (timeLastSizeUpdate < ver.getTimeLastSizeUpdate());
    }else{
        return (size < ver.getSize());
    }
}

bool ReplicaVersion::operator>(ReplicaVersion& ver){
    return( !( *this < ver || *this == ver));
}

bool ReplicaVersion::operator==(ReplicaVersion& ver){
   if(time1 == ver.getTime1() &&
      time2 == ver.getTime2() &&
      time3 == ver.getTime3() &&
      timeLastSizeUpdate == ver.getTimeLastSizeUpdate() &&
      size == ver.getSize()){
        return true;
    }
    return false;
}

bool ReplicaVersion::operator<=(ReplicaVersion& ver){
    return (*this < ver || *this == ver);
}

bool ReplicaVersion::operator>=(ReplicaVersion& ver){
    return (*this > ver || *this == ver);
}

void ReplicaVersion::operator=(ReplicaVersion& ver){
    time1 = ver.getTime1();
    time2 = ver.getTime2() ;
    time3 = ver.getTime3() ;
    timeLastSizeUpdate = ver.getTimeLastSizeUpdate() ;
    size = ver.getSize();
}


char* ReplicaVersion::versionToSendString(){
    char str[1000];
    snprintf(str,1000,"%ld.%ld.%ld.%ld.%ld",time1,time2,time3,timeLastSizeUpdate,size);
/*    str << time1 << "."
        << time2 << "."
        << time3 << "."
        << timeLastSizeUpdate) << "."
        << size << endl;
*/
    char* strToReturn = strdup(str);
    return strToReturn;
}

char* ReplicaVersion::versionToPrintString(){
    char str[1000];
/*    str << "Time1: " << ctime(time1) << "\n"
        << "Time2: " << ctime(time2) << "\n"
        << "Time3: " << ctime(time3) << "\n"
        << "TimeLastSizeUpdate: " << ctime(TimeLastSizeUpdate) << "\n"
        << "Size: " << ctime(size) << endl;
*/

    snprintf(str,1000,"Time1: %ld\n Time2: %ld \nTime3: %ld \nLastUpdated: %ld \nSize: %ld",time1,time2,time3,timeLastSizeUpdate,size);
    char* strToReturn = strdup(str);
    return strToReturn;
}






