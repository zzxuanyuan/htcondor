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

#include "condor_common.h"
#include "KeyCache.h"
#include "CryptKey.h"

KeyCacheEntry::KeyCacheEntry( char *id, struct sockaddr_in * addr, const char * user, KeyInfo* key, int expiration) {
	_id = strdup(id);
	_addr = new struct sockaddr_in(*addr);
    _user = user == 0 ? 0 : strdup(user);
	_key = new KeyInfo(*key);
	_expiration = expiration;
}

KeyCacheEntry::KeyCacheEntry(const KeyCacheEntry& copy) 
{
	_id = strdup(copy._id);
    _addr = new struct sockaddr_in(*(copy._addr));
    _user = copy._user == 0 ? 0 : strdup(copy._user);
	_key = new KeyInfo(*(copy._key));
	_expiration = copy._expiration;
}

KeyCacheEntry::~KeyCacheEntry() {
	delete_storage();
}

KeyCacheEntry& KeyCacheEntry::operator=(const KeyCacheEntry &copy) {
	if (this != &copy) {
		delete_storage();
		_id = strdup(copy._id);
		_addr = new struct sockaddr_in(*(copy._addr));
        _user = copy._user == 0 ? 0 : strdup(copy._user);
		_key = new KeyInfo(*(copy._key));
		_expiration = copy._expiration;
	}
	return *this;
}

char* KeyCacheEntry::id() {
	return _id;
}

struct sockaddr_in *  KeyCacheEntry::addr() {
	return _addr;
}

KeyInfo* KeyCacheEntry::key() {
	return _key;
}

int KeyCacheEntry::expiration() {
	return _expiration;
}

const char * KeyCacheEntry :: user() 
{
    return _user;
}

void KeyCacheEntry::delete_storage() {
	if (_id) {
	  delete _id;
	}
	if (_addr) {
	  delete _addr;
	}
	if (_key) {
	  delete _key;
	}
    if (_user) {
        delete _user;
    }
}

KeyCache::KeyCache(int nbuckets) {
	key_table = new HashTable<MyString, KeyCacheEntry*>(nbuckets, MyStringHash, rejectDuplicateKeys);
}

KeyCache::KeyCache(const KeyCache& k) {
	key_table = new HashTable<MyString, KeyCacheEntry*>(*(k.key_table));
}

KeyCache::~KeyCache() {
	delete_storage();
}

	    
KeyCache& KeyCache::operator=(const KeyCache& k) {
	if (this != &k) {
		delete_storage();
		key_table = new HashTable<MyString, KeyCacheEntry*>(*(k.key_table));
	}
	return *this;
}

void KeyCache::delete_storage() {
	if (key_table) {
		// Delete all entries from the hash, and the table itself
		KeyCacheEntry* key_entry;
		key_table->startIterations();
		while (key_table->iterate(key_entry)) {
			if ( key_entry ) delete key_entry;
		}

		delete key_table;
		key_table = NULL;
	}
}

bool KeyCache::insert(char *key_id, KeyCacheEntry &e) {
	return key_table->insert(key_id, new KeyCacheEntry(e));
}

bool KeyCache::lookup(char *key_id, KeyCacheEntry *&e_ptr) {

	bool res = key_table->lookup(key_id, e_ptr);

	if (res) {
		// automatically check the expiration of this key

		// draw the line
		time_t cutoff_time = time(0);

		if (e_ptr && e_ptr->expiration() && (e_ptr->expiration() <= cutoff_time) ) {
			expire(e_ptr);
			res = false;
		}
	}

	return res;
}

bool KeyCache::remove(char *key_id) {
	return key_table->remove(key_id);
}

void KeyCache::expire(KeyCacheEntry *e) {
	// the pointer they passed in could be pointing
	// to the string within this object, so we need
	// to keep the info we want before we delete it
	char* key_id = strdup (e->id());
	time_t key_exp = e->expiration();

	dprintf (D_SECURITY, "KEYCACHE: Key %s expired at %s.\n", e->id(), ctime(&key_exp) );

	// delete the object
	delete e;

	// remove its reference from the hash table
	remove(key_id);
	dprintf (D_SECURITY, "KEYCACHE: Removed %s from key cache.\n", key_id);

	delete key_id;
}

void KeyCache::RemoveExpiredKeys() {

	// draw the line
	time_t cutoff_time = time(0);

	// Delete all entries from the hash, and the table itself
	KeyCacheEntry* key_entry;
	key_table->startIterations();
	while (key_table->iterate(key_entry)) {
		if (key_entry->expiration() && key_entry->expiration() < cutoff_time) {
			expire(key_entry);
		}
	}
}

