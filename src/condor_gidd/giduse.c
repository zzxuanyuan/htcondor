/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "err.h"
#include "giduse.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// helper function to create a path of the form: /proc/<pid>/status. returns
// NULL if a failure occurs, otherwise a malloc-allocated buffer with the
// path is returned
//
static char*
statuspath(char* pid)
{
	char proc[] = "/proc/";
	char status[] = "/status";
	int proclen = sizeof(proc) - 1;
	int pidlen = strlen(pid);
	int statuslen = sizeof(status) - 1;

	char* path = malloc(proclen + pidlen + statuslen + 1);
	if (path == NULL) {
		err_sprintf("malloc failure: %s", strerror(errno));
		return NULL;
	}

	memcpy(path, proc, proclen);
	memcpy(path + proclen, pid, pidlen);
	memcpy(path + proclen + pidlen, status, statuslen);
	path[proclen + pidlen + statuslen] = '\0';

	return path;
}

// helper type for a node in a list of GIDs
//
typedef struct gidnode {
	gid_t gid;
	struct gidnode* next;
} gidnode_t;

// helper type to build up a list of GIDs incrementally
//
typedef struct {
	gidnode_t* head;
	int count;
} gidlist_t;

// initialize a gidlist_t
//
static void
listinit(gidlist_t* list) {
	list->head = NULL;
	list->count = 0;
}

// add a GID to a gidlist_t. returns 0 on success, -1 on failure
//
static int
listadd(gidlist_t* list, gid_t gid)
{
	gidnode_t* node = malloc(sizeof(gidnode_t));
	if (node == NULL) {
		err_sprintf("malloc failure: %s", strerror(errno));
		return -1;
	}
	node->gid = gid;
	node->next = list->head;
	list->head = node;
	list->count++;
	return 0;
}

// free up any memory being used by a gidlist_t
//
static void
listfree(gidlist_t* list)
{
	gidnode_t* node = list->head;
	while (node != NULL) {
		gidnode_t* next = node->next;
		free(node);
		node = next;
	}
}

// helper function to get the set of supplementary group IDs for a given
// PID. returns 0 on success, -1 on failure. on success, the list parameter
// will return a gidlist_t with all the GIDs
//
static int
getgids(char* pid, gidlist_t* list)
{
	char* path = statuspath(pid);
	if (path == NULL) {
		return -1;
	}

	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		err_sprintf("fopen failure on %s: %s", path, strerror(errno));
		free(path);
		return -1;
	}

	int found = 0;
	char grps[] = "Groups:\t";
	int grpslen = sizeof(grps) - 1;
	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, grps, grpslen) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		if (feof(fp)) {
			err_sprintf("groups not found in %s", path);
		}
		else {
			err_sprintf("fgets error on %s: %s",
			            path,
			            strerror(errno));
		}
		free(path);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	int linelen = strlen(line);
	if (line[linelen - 1] != '\n') {
		err_sprintf("partial read of groups line in %s", path);
		free(path);
		return -1;
	}

	listinit(list);
	char* ptr = line + grpslen;
	while (*ptr != '\n') {
		char* end;
		gid_t gid = (gid_t)strtoul(ptr, &end, 10);
		if ((end == ptr) || (*end != ' ')) {
			err_sprintf("unexpected format for groups in %s",
			            path);
			free(path);
			listfree(list);
			return -1;
		}
		if (listadd(list, gid) == -1) {
			free(path);
			listfree(list);
			return -1;
		}
		ptr = end + 1;
	}
	free(path);

	return 0;
}

int
giduse_probe(gid_t first, int count, int* used)
{
	DIR* dir = opendir("/proc");
	if (dir == NULL) {
		err_sprintf("opendir failure on /proc: %s", strerror(errno));
		return -1;
	}

	for (int i = 0; i < count; i++) {
		used[i] = 0;
	}

	while (1) {

		struct dirent* de = readdir(dir);
		if (de == NULL) {
			break;
		}

		if (!isdigit(de->d_name[0])) {
			continue;
		}

		gidlist_t list;
		if (getgids(de->d_name, &list) == -1) {
			continue;
		}

		gidnode_t* node = list.head;
		while (node != NULL) {
			int i = node->gid - first;
			if ((i >= 0) && (i < count)) {
				used[i]++;
			}
			node = node->next;
		}

		listfree(&list);
	}

	closedir(dir);

	return 0;
}
