/***************************************************************************
            berkeleydb.c  -  A Berkeley DB backend for Elektra
                             -------------------
    begin                : Mon Jan 24 2005
    copyright            : (C) 2005 by Avi Alkalay
    email                : avi@unix.sh
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the BSD License (revised).                      *
 *                                                                         *
 ***************************************************************************/




/* Subversion stuff

$Id$
$LastChangedBy$

*/



#include <kdb.h>
#include <kdbbackend.h>
#include <db.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

#define BACKENDNAME "berkeleydb"

#define DB_DIR_USER   ".kdb-berkeleydb"
#define DB_DIR_SYSTEM "/etc/kdb-berkeleydb"

#define DB_FILE_KEYVALUE   "keyvalue.db"
#define DB_FILE_PARENTS    "parents.idx"

/**Some systems have even longer pathnames */
#ifdef PATH_MAX
#define MAX_PATH_LENGTH PATH_MAX
/**This value is garanteed on any Posix system */
#elif __USE_POSIX
#define MAX_PATH_LENGTH _POSIX_PATH_MAX
#else 
#define MAX_PATH_LENGTH 4096
#endif




/**
 *  A container for the Berkeley DBs related to the same DBTree.
 */
typedef struct {
	DB *parentIndex;   /* maps folders to the keys they contain */
	DB *keyValuePairs; /* maps keynames to their values + metainfo */
} DBInternals;





/**
 *  Each DBTree contains all info needed to access an Elektra
 *  root tree.
 *  
 *  Example of root trees:
 *  system/ *        {isSystem=1,userDomain=0,...}
 *  user/ *          {isSystem=0,userDomain=$USER,...}
 *  user:luciana/ *  {isSystem=0,userDomain=luciana,...}
 *  user:denise/ *   {isSystem=0,userDomain=denise,...}
 *  user:tatiana/ *  {isSystem=0,userDomain=tatiana,...}
 *  
 */
typedef struct _DBTree {
	/* if isSystem==0 and userDomain==0, this DB is for the current user */
	int isSystem;
	char *userDomain;
	DBInternals db;
	struct _DBTree *next;
} DBTree;






/**
 *  A container for all opened DBTrees
 */
typedef struct {
	size_t size;       /* number of opened databases */
	DBTree *cursor;
	DBTree *first;     /* databases */
} DBContainer;



DBContainer *dbs=0;





int keyToBDB(Key *key, DBT *dbkey, DBT *dbdata) {
	void *serialized;
	size_t metaInfoSize;

	memset(dbkey, 0, sizeof(DBT));
	memset(dbdata, 0, sizeof(DBT));

	metaInfoSize=
		  sizeof(key->type)
		+ sizeof(key->uid)
		+ sizeof(key->gid)
		+ sizeof(key->access)
		+ sizeof(key->atime)
		+ sizeof(key->mtime)
		+ sizeof(key->ctime)
		+ sizeof(key->commentSize)
		+ sizeof(key->dataSize);

	
	dbdata->size = metaInfoSize + key->dataSize + key->commentSize;
	serialized = malloc(dbdata->size);

	/* First part: the metainfo */
	memcpy(serialized,key,metaInfoSize);

	/* Second part: the comment */
	memcpy(serialized+metaInfoSize,key->comment,key->commentSize);

	/* Third part: the value */
	memcpy(serialized+metaInfoSize+key->commentSize,key->data,key->dataSize);
	
	dbdata->data=serialized;

	dbkey->size=strblen(key->key);
	dbkey->data=malloc(dbkey->size);
	strcpy(dbkey->data,key->key);
	
	return 0;
}






int keyFromBDB(Key *key, DBT *dbkey, DBT *dbdata) {
	size_t metaInfoSize;
	
	metaInfoSize=
		  sizeof(key->type)
		+ sizeof(key->uid)
		+ sizeof(key->gid)
		+ sizeof(key->access)
		+ sizeof(key->atime)
		+ sizeof(key->mtime)
		+ sizeof(key->ctime)
		+ sizeof(key->commentSize)
		+ sizeof(key->dataSize);
	
	keyClose(key);
	memset(key,0,sizeof(key));
	
	/* Set all metainfo */
	memcpy(key,          /* destination */
		dbdata->data,    /* source */
		metaInfoSize);   /* size */
	
	key->flags = KEY_SWITCH_INITIALIZED;

	/* Set key name. TODO: not needed... we can hack internally for speed */
	keySetName(key,dbkey->data);
	
	/* Set comment */
	if (key->commentSize)
		keySetComment(key,dbdata->data+metaInfoSize);
	
	/* Set value. Key type came from the metaInfo importing above. */
	keySetRaw(key,dbdata->data+metaInfoSize+key->commentSize,key->dataSize);
	
	/* since we just got the key from the storage structure, it is synced. */
	key->flags &= ~KEY_SWITCH_NEEDSYNC;
	
	/* userDomain must be set outside this function,
	 * someplace more aware of the context */
	
	return 0;
}






int parentIndexCallback(DB *db, const DBT *rkey, const DBT *rdata, DBT *pkey) {
	size_t baseNameSize,parentNameSize;
	char *parentPrivateCopy=0;

	baseNameSize=keyNameGetBaseNameSize(rkey->data);
	if (baseNameSize == 0) return DB_DONOTINDEX;

	memset(pkey, 0, sizeof(DBT));

	parentNameSize=strblen(rkey->data)-baseNameSize;
	parentPrivateCopy=malloc(parentNameSize);

	if (parentPrivateCopy) {
		memcpy(parentPrivateCopy,rkey->data,parentNameSize-1);
		parentPrivateCopy[parentNameSize-1]=0;
	}

	pkey->data=parentPrivateCopy;
	pkey->size=parentNameSize;
	pkey->flags=DB_DBT_APPMALLOC;

	return 0;
}








int dbTreeDel(DBTree *dbtree) {
	if (dbtree->userDomain) free(dbtree->userDomain);
	if (dbtree->db.keyValuePairs)
		dbtree->db.keyValuePairs->close(dbtree->db.keyValuePairs,0);
	if (dbtree->db.parentIndex)
		dbtree->db.parentIndex->close(dbtree->db.parentIndex,0);
	
	free(dbtree);
	
	return 0;
}




/**
 * Given a created, opened and empty DBTree, initialize its root key.
 * This is usually called by dbTreeNew().
 */
int dbTreeInit(DBTree *newDB) {
	Key *root=0;
	int ret;
	DBT dbkey,data;

	
	if (newDB->isSystem) {
		root=keyNew("system",
			KEY_SWITCH_UID,0,
			KEY_SWITCH_GID,0,
			KEY_SWITCH_TYPE,KEY_TYPE_DIR,
			KEY_SWITCH_END);
	} else {
		struct passwd *userOwner;
		userOwner=getpwnam(newDB->userDomain);
		root=keyNew("user",
			KEY_SWITCH_TYPE,KEY_TYPE_DIR,
			KEY_SWITCH_END);
	}

	root->atime=root->mtime=root->ctime=time(0); /* set current time */

	keyToBDB(root,&dbkey,&data);

	ret = newDB->db.keyValuePairs->put(newDB->db.keyValuePairs,
		0,&dbkey,&data, 0);
	if (!ret) printf("db: %s: DB Initialized.\n", (char *)dbkey.data);
	else {
		newDB->db.keyValuePairs->err(newDB->db.keyValuePairs, ret, "DB->put");
		perror("DB->put");
	}

	keyDel(root);
	free(dbkey.data); dbkey.data=0;
	free(data.data); data.data=0;

	newDB->db.keyValuePairs->sync(newDB->db.keyValuePairs,0);

	return KDB_RET_OK;
}






/**
 * Tries to open a DB for a key.
 * If it doesn't exist, try to create it.
 * The returned new DBTree must be included in the static single
 * DBContainer by the caller.
 */
DBTree *dbTreeNew(Key *forKey) {
	DBTree *newDB;
	int ret;
	int newlyCreated; /* True if this is a new database */

	char dbDir[MAX_PATH_LENGTH];
	char hier[MAX_PATH_LENGTH];
	char keys[MAX_PATH_LENGTH];
	struct passwd *user=0;

	struct stat dbDirInfo;


	/***********
	 * Calculate path and filenames for the DB files.
	 ***********/
	  
	if (keyIsSystem(forKey)) {
		/* Prepare to open the 'system/ *' database */
		strcpy(dbDir,DB_DIR_SYSTEM);
	} else if (keyIsUser(forKey)) {
		/* Prepare to open the 'user:????.*' database */
		user=getpwnam(forKey->userDomain);
		sprintf(dbDir,"%s/%s",user->pw_dir,DB_DIR_USER);
	}

	if (stat(dbDir,&dbDirInfo)) {
		/* Directory does not exist. create it */
		int ret;
		
		printf("Going to create dir %s\n",dbDir);
		ret=mkdir(dbDir,DEFFILEMODE);
		if (ret) return 0; /* propagate errno */
	} else {
		/* Something exist there. Check it first */
		if (!S_ISDIR(dbDirInfo.st_mode)) {
			/* It is not a directory ! */
			errno=EACCES;
			return 0;
		}
	}

	sprintf(keys,"%s/%s",dbDir,DB_FILE_KEYVALUE);
	sprintf(hier,"%s/%s",dbDir,DB_FILE_PARENTS);

	newDB=malloc(sizeof(DBTree));
	memset(newDB,0,sizeof(DBTree));
	newDB->isSystem=keyIsSystem(forKey);
	newlyCreated=0;


	/* We have the files names. Now open/create them */

	/****************
	 * The main database. The one you can find the real key-value pairs
	 *****************/
	if ((ret = db_create(&newDB->db.keyValuePairs, NULL, 0)) != 0) {
		fprintf(stderr, "db_create: %s: %s\n", keys, db_strerror(ret));
		free(newDB);
		errno=KDB_RET_EBACKEND;
		return 0;
	}

	ret=newDB->db.keyValuePairs->open(newDB->db.keyValuePairs,NULL,keys,
		NULL, DB_BTREE, DB_CREATE | DB_EXCL  | DB_THREAD, 0600);
	if (ret != EEXIST) newlyCreated=1;
	else {
		/* DB already exist. Only open it */
		ret=newDB->db.keyValuePairs->open(newDB->db.keyValuePairs,NULL, keys,
			NULL, DB_BTREE, DB_THREAD, 0);
	}

	if (ret) {
		newDB->db.keyValuePairs->err(newDB->db.keyValuePairs, ret,
			"%s", keys);
		dbTreeDel(newDB);
		errno=KDB_RET_EBACKEND;
		return 0;
	}




	/* TODO: Check newlyCreated also */
	/****************
	 * The parent index. To make key searches by their parents
	 *****************/
	ret=db_create(&newDB->db.parentIndex, NULL, 0);
	if (ret != 0) {
		fprintf(stderr, "db_create: %s: %s\n", hier, db_strerror(ret));
		dbTreeDel(newDB);
		errno=KDB_RET_EBACKEND;
		return 0;
	}
	
	ret = newDB->db.parentIndex->set_flags(newDB->db.parentIndex,
		DB_DUP | DB_DUPSORT);
	if (ret != 0) fprintf(stderr, "set_flags: %s: %d\n",hier,ret);
	
	ret = newDB->db.parentIndex->open(newDB->db.parentIndex,
		NULL, hier, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0600);
	if (ret != 0) {
		newDB->db.parentIndex->err(newDB->db.parentIndex, ret, "%s", hier);
		dbTreeDel(newDB);
		errno=KDB_RET_EBACKEND;
		return 0;
	}
	
	ret = newDB->db.keyValuePairs->associate(newDB->db.keyValuePairs, NULL,
		newDB->db.parentIndex, parentIndexCallback, DB_DBT_APPMALLOC);
	if (ret != 0) {
		fprintf(stderr, "error: %s: %d\n",hier,ret);
		dbTreeDel(newDB);
		errno=KDB_RET_EBACKEND;
		return 0;
	}




	if (!newDB->isSystem) {
		newDB->userDomain=malloc(strblen(forKey->userDomain));
		strcpy(newDB->userDomain,forKey->userDomain);
	}

	/* Set file permissions for the DB files */
	if (newlyCreated) {
		if (user) {
			chown(hier,  user->pw_uid,user->pw_gid);
			chown(keys,  user->pw_uid,user->pw_gid);
		}
		dbTreeInit(newDB); /* populate */
	}
	return newDB;
}









/**
 * Return the DB suitable for the key.
 * Lookup in the list of opened DBs (DBContainer). If not found, tries to
 * open it with dbTreeNew().
 */
DBTree *getDBForKey(Key *key) {
	DBTree *current,*newDB;
	char rootName[100];
	rootName[0]=0; /* just to be sure... */

	if (dbs->cursor) current=dbs->cursor;
	else current=dbs->cursor=dbs->first;
	
	/* We found some DB opened.
	 * Browse it starting from the cursor. */
	if (current) {
		/* Look for a DB in our opened DBs */
		if (keyIsSystem(key))
			do {
				if (current->isSystem) return dbs->cursor=current;
			
				current=current->next;
				if (!current) current=dbs->first;
			} while (current && current!=dbs->cursor);
		else if (keyIsUser(key))
			do {
				if (!current->isSystem && !strcmp(key->userDomain,current->userDomain))
					return dbs->cursor=current;
				
				current=current->next;
				if (!current) current=dbs->first;
			} while (current && current!=dbs->cursor);
	}
	
	/* If we reached this point, the DB for our key is not it our container.
	 * Open it and include in the container. */

	newDB=dbTreeNew(key);
	if (newDB) {
		/* Put the new DB right after the container's current DB (cursor).
		 * And set the cursor to be the new DB. */
		if (dbs->cursor) {
			newDB->next=dbs->cursor->next;
			dbs->cursor->next=newDB;
			dbs->cursor=newDB;
		} else dbs->cursor=dbs->first=newDB;
		dbs->size++;
	}
	
	/* If some error ocurred inside dbTreeNew(), errno will be propagated */
	
	return dbs->cursor;
}













int kdbOpen_bdb() {
	/* Create only the DB container.
	 * DBs will be allocated on demand
	 */
	dbs=malloc(sizeof(DBContainer));
	memset(dbs,0,sizeof(DBContainer));
	return 0;
}




int kdbClose_bdb() {
	if (dbs) {
		while (dbs->first) {
			dbs->cursor=dbs->first;
			dbs->first=dbs->cursor->next;

			dbTreeDel(dbs->cursor);
		}
	}
	return 0; /* success */
}




int kdbStatKey_bdb(Key *key) {
	/* get the most possible key metainfo */
	return 0; /* success */
}




int kdbGetKey_bdb(Key *key) {
	DBTree *dbctx;
	DBT dbkey,data;
	int ret;
	uid_t user=getuid();
	gid_t group=getgid();
	int canRead=0;

	dbctx=getDBForKey(key);
	if (!dbctx) return 1; /* propagate errno from getDBForKey() */


	memset(&dbkey,0,sizeof(DBT));
	memset(&data,0,sizeof(DBT));
	dbkey.size=dbkey.ulen=strblen(key->key);
	dbkey.data=key->key;
	dbkey.flags=data.flags=DB_DBT_REALLOC;

	ret = dbctx->db.keyValuePairs->get(dbctx->db.keyValuePairs,
		NULL, &dbkey, &data, 0);
		
	switch (ret) {
		case 0: { /* Key found and retrieved. Check permissions */
			Key buffer;

			keyInit(&buffer);
			keyFromBDB(&buffer,&dbkey,&data);
			
			/* Check permissions. */
			if (keyGetUID(&buffer) == user)
				canRead = keyGetAccess(&buffer) & S_IRUSR;
			else if (keyGetGID(&buffer) == group)
				canRead = keyGetAccess(&buffer) & S_IRGRP;
			else canRead= keyGetAccess(&buffer) & S_IROTH;

			if (canRead) {
				keyDup(&buffer,key);
				keyClose(&buffer);
				return 0;
			} else {
				keyClose(&buffer);
				return errno=KDB_RET_NOCRED;
			}
			break;
		}
		case DB_NOTFOUND:
			return errno=KDB_RET_NOTFOUND;
			break;
	}
	
	return 0; /* success, but we'll never reach this point */
}



/**
 * Implementation for kdbSetKey() method.
 *
 * @see kdbSetKey() for expected behavior.
 * @ingroup backend
 */
int kdbSetKey_bdb(Key *key) {
	DBTree *dbctx;
	DBT dbkey,data;
	int ret;
	uid_t user=getuid();
	gid_t group=getgid();
	int canWrite=0;

	dbctx=getDBForKey(key);
	if (!dbctx) return 1; /* propagate errno from getDBForKey() */

	/* Check access permissions.
	   Check if this client can commit this key to the database */

	memset(&dbkey,0,sizeof(DBT));
	memset(&data,0,sizeof(DBT));
	dbkey.size=dbkey.ulen=strblen(key->key);
	dbkey.data=key->key;
	dbkey.flags=data.flags=DB_DBT_REALLOC;

	ret = dbctx->db.keyValuePairs->get(dbctx->db.keyValuePairs,
		NULL, &dbkey, &data, 0);
		
	switch (ret) {
		case 0: { /* Key found and retrieved. Check permissions */
			Key buffer;
			
			keyInit(&buffer);
			keyFromBDB(&buffer,&dbkey,&data);
			keySetOwner(&buffer,dbctx->userDomain);

			/* Check parent permissions to write bellow it. */
			if (keyGetUID(&buffer) == user)
				canWrite = keyGetAccess(&buffer) & S_IWUSR;
			else if (keyGetGID(&buffer) == group)
				canWrite = keyGetAccess(&buffer) & S_IWGRP;
			else canWrite= keyGetAccess(&buffer) & S_IWOTH;
			
			keyClose(&buffer);
			break;
		}
		case DB_NOTFOUND: {
			/* We don't have this key yet.
			   Check if we have a parent and its permissions. */
			Key *parent=0;
			u_int32_t parentNameSize;
			char *parentName;

			parentNameSize=keyGetParentNameSize(key);
			parentName=malloc(parentNameSize+1); /* includes the leading \0 */
			keyGetParentName(key,parentName,parentNameSize+1);
			
			memset(&dbkey,0,sizeof(DBT));
			memset(&data,0,sizeof(DBT));
			dbkey.data=parentName;
			dbkey.size=parentNameSize+1;
			dbkey.flags=data.flags=DB_DBT_REALLOC;

			ret = dbctx->db.keyValuePairs->get(dbctx->db.keyValuePairs, NULL,
				&dbkey, &data, 0);

			if (ret == DB_NOTFOUND) {
				/* No, we don't have a parent. Create dirs recursivelly */
				
				/* umask etc will be the defaults for current user */
				parent=keyNew(parentName,
					KEY_SWITCH_TYPE,KEY_TYPE_DIR,
					KEY_SWITCH_END);
				
				if (kdbSetKey_bdb(parent))
					/* Some error happened in this recursive call.
					 * Propagate errno.
					 */
					return 1;
			} else {
				/* Yes, we have a parent already. */
				parent=keyNew(0);
				keyFromBDB(parent,&dbkey,&data);
				keySetOwner(parent,dbctx->userDomain);
			}

			/* Check parent permissions to write bellow it. */
			if (keyGetUID(parent) == user)
				canWrite = keyGetAccess(parent) & S_IWUSR;
			else if (keyGetGID(parent) == group)
				canWrite = keyGetAccess(parent) & S_IWGRP;
			else canWrite= keyGetAccess(parent) & S_IWOTH;
			
			keyDel(parent);
			break;
		}
	}

	if (! canWrite) return errno=KDB_RET_NOCRED;

	key->mtime=key->atime=time(0); /* set current time into key */
	keyToBDB(key,&dbkey,&data);

	if ((ret = dbctx->db.keyValuePairs->put(dbctx->db.keyValuePairs,
			NULL, &dbkey, &data, 0)) != 0) {
		dbctx->db.keyValuePairs->err(dbctx->db.keyValuePairs, ret, "DB->put");
		
		free(dbkey.data); dbkey.data=0;
		free(data.data); data.data=0;

		errno=KDB_RET_NOCRED; /* probably this is the error */
		return 1;
	}

	/* Mark the key as synced */
	key->flags &= ~KEY_SWITCH_NEEDSYNC;

	free(dbkey.data); dbkey.data=0;
	free(data.data); data.data=0;

	dbctx->db.keyValuePairs->sync(dbctx->db.keyValuePairs,0);
	
	return 0; /* success */
}



/**
 * Implementation for kdbRename() method.
 *
 * @see kdbRename() for expected behavior.
 * @ingroup backend
 */
int kdbRename_backend(Key *key, const char *newName) {
	/* rename a key to another name */
	return 0; /* success */
}




/**
 * Implementation for kdbRemoveKey() method.
 *
 * @see kdbRemove() for expected behavior.
 * @ingroup backend
 */
int kdbRemoveKey_backend(const Key *key) {
	/* remove a key from the database */
	return 0;  /* success */
}




/**
 * Implementation for kdbGetKeyChildKeys() method.
 *
 * @see kdbGetKeyChildKeys() for expected behavior.
 * @ingroup backend
 */
int kdbGetKeyChildKeys_bdb(const Key *parentKey, KeySet *returned, unsigned long options) {
	/* retrieve multiple hierarchical keys */
	return 0; /* success */
}





/**
 * The implementation of this method is optional.
 * The builtin inefficient implementation will use kdbGetKey() for each
 * key inside @p interests.
 *
 * @see kdbMonitorKeys() for expected behavior.
 * @ingroup backend
 */
u_int32_t kdbMonitorKeys_backend(KeySet *interests, u_int32_t diffMask,
		unsigned long iterations, unsigned sleep) {
	return 0;
}



/**
 *
 * The implementation of this method is optional.
 * The builtin inefficient implementation will use kdbGetKey() for
 * @p interest.
 *
 * @see kdbMonitorKey() for expected behavior.
 * @ingroup backend
 */
u_int32_t kdbMonitorKey_backend(Key *interest, u_int32_t diffMask,
		unsigned long iterations, unsigned sleep) {
	return 0;
}


/**
 * All KeyDB methods implemented by the backend can have random names, except
 * kdbBackendFactory(). This is the single symbol that will be looked up
 * when loading the backend, and the first method of the backend
 * implementation that will be called.
 * 
 * Its purpose is to "publish" the exported methods for libkdb.so. The
 * implementation inside the provided skeleton is usually enough: simply
 * call kdbBackendExport() with all methods that must be exported.
 * 
 * @return whatever kdbBackendExport() returns
 * @see kdbBackendExport() for an example
 * @see kdbOpenBackend()
 * @ingroup backend
 */
KDBBackend *kdbBackendFactory(void) {
	return kdbBackendExport(BACKENDNAME,
		KDB_BE_OPEN,           &kdbOpen_bdb,
		KDB_BE_CLOSE,          &kdbClose_bdb,
		KDB_BE_GETKEY,         &kdbGetKey_bdb,
		KDB_BE_SETKEY,         &kdbSetKey_bdb,
/*		KDB_BE_STATKEY,        &kdbStatKey_bdb,
		KDB_BE_RENAME,         &kdbRename_backend,
		KDB_BE_REMOVEKEY,      &kdbRemoveKey_backend, */
		KDB_BE_GETCHILD,       &kdbGetKeyChildKeys_bdb,
		/* set to default implementation: */
		KDB_BE_MONITORKEY,     &kdbMonitorKey_default,
		KDB_BE_MONITORKEYS,    &kdbMonitorKeys_default,
		KDB_BE_SETKEYS,        &kdbSetKeys_default,
		KDB_BE_END);
}
