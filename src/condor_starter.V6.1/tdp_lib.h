#ifndef TDP_LIB_H
#define TDP_LIB_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/user.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>

#define TDP_ENV_SERVADDR "TDP_AS_ADDR"

#define BUFFER_SIZE 1024

/*Identificators of messages:
	0=Put
	1=Get
	2=Async_Put
	3=Async_Get
	4=Unput
	5=Confirmation Syncronous Put or Get
	6=Syncronous Put
	7=Syncronous Get
*/

#define MSG_PUT 0
#define MSG_GET 1
#define MSG_ASYNC_PUT 2
#define MSG_ASYNC_GET 3
#define MSG_UNPUT 4
#define MSG_CONFIRM_PUT_GET 5
#define MSG_SYNC_PUT 6
#define MSG_SYNC_GET 7

/* formerly defined in tdp_types.h */
typedef pid_t id_proccess;
typedef void (*CallBack)();

/*Types of process creation*/
typedef enum {CREATE_PROCESS,PAUSSED_PROCESS} type_process_creation;


typedef struct {
	int id_handle;
	int tdp_chanel;
}TDP_handle;
	

/*General*/

int TDP_init(TDP_handle *tdp_handle );

int TDP_test();

/*Process creation & management */

int TDP_trace_me(id_proccess pid);

int TDP_wait_stopped_child (id_proccess pid);

id_proccess TDP_create_process (type_process_creation proc_creation,char *name, char *args[], char *env[], CallBack func);

int TDP_attach_to_stopped_process (id_proccess pid);

int TDP_continue_stopped_process (id_proccess pid);

/* Puts & gets syncronous and assincronous */

int TDP_put(TDP_handle tdp_handle, char *attribute, char *value);

int TDP_async_put(TDP_handle tdp_handle, char *attribute, char *value,void (*func) (char *), char *args);

int TDP_sync_put(TDP_handle tdp_handle, char *attribute, char *value);

int TDP_async_get(TDP_handle tdp_handle, char *attribute, void (*func) (char *), char *args);

int TDP_sync_get(TDP_handle tdp_handle, char *attribute, char *value);

int TDP_get(TDP_handle tdp_handle, char *attribute, char *value);

int TDP_unput(TDP_handle tdp_handle, char *attribute);

int TDP_async_test(TDP_handle tdp_handle,char *attribute, char *value, int *type);

#endif
