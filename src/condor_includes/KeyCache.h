/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_KEYCACHE_H_INCLUDE
#define CONDOR_KEYCACHE_H_INCLUDE

#include "condor_common.h"
#include "CryptKey.h"
#include "MyString.h"
#include "HashTable.h"

class KeyCacheEntry {
 public:
    KeyCacheEntry(
			char * id,
			struct sockaddr_in * addr,
			KeyInfo* key,
			int expiration
			);
    KeyCacheEntry(const KeyCacheEntry &copy);
    ~KeyCacheEntry();

	KeyCacheEntry& operator=(const KeyCacheEntry &kc);

    char*                 id();
    struct sockaddr_in *  addr();
    KeyInfo*              key();
    int                   expiration();

 private:

	void delete_storage();

    char *               _id;
    struct sockaddr_in * _addr;
    KeyInfo*             _key;
    int                  _expiration;
};



class KeyCache {
public:
	KeyCache(int nbuckets);
	KeyCache(const KeyCache&);
	~KeyCache();
	
	KeyCache& operator=(const KeyCache&);

	bool insert(char *key_id, KeyCacheEntry&);
	bool lookup(char *key_id, KeyCacheEntry*&);
	bool remove(char *key_id);
	void expire(KeyCacheEntry*);

	void RemoveExpiredKeys();

private:
	void delete_storage();

	HashTable<MyString, KeyCacheEntry*> *key_table;
};


#endif
