/*-------------------------------------------------------------------------
 *
 * postinit.c
 *	  postgres initialization utilities
 *
 * Portions Copyright (c) 1996-2000, PostgreSQL, Inc
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header$
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <math.h>
#include <unistd.h>

#ifndef OLD_FILE_NAMING
#include "catalog/catalog.h"
#endif

#include "access/heapam.h"
#include "catalog/catname.h"
#include "catalog/pg_database.h"
#include "miscadmin.h"
#include "storage/backendid.h"
#include "storage/proc.h"
#include "storage/sinval.h"
#include "storage/smgr.h"
#include "utils/fmgroids.h"
#include "utils/portal.h"
#include "utils/relcache.h"
#include "utils/syscache.h"

#ifdef MULTIBYTE
#include "mb/pg_wchar.h"
#endif

static void ReverifyMyDatabase(const char *name);
static void InitCommunication(void);

static IPCKey PostgresIpcKey;

/*** InitPostgres support ***/


/* --------------------------------
 *		ReverifyMyDatabase
 *
 * Since we are forced to fetch the database OID out of pg_database without
 * benefit of locking or transaction ID checking (see utils/misc/database.c),
 * we might have gotten a wrong answer.  Or, we might have attached to a
 * database that's in process of being destroyed by destroydb().  This
 * routine is called after we have all the locking and other infrastructure
 * running --- now we can check that we are really attached to a valid
 * database.
 *
 * In reality, if destroydb() is running in parallel with our startup,
 * it's pretty likely that we will have failed before now, due to being
 * unable to read some of the system tables within the doomed database.
 * This routine just exists to make *sure* we have not started up in an
 * invalid database.  If we quit now, we should have managed to avoid
 * creating any serious problems.
 *
 * This is also a handy place to fetch the database encoding info out
 * of pg_database, if we are in MULTIBYTE mode.
 * --------------------------------
 */
static void
ReverifyMyDatabase(const char *name)
{
	Relation	pgdbrel;
	HeapScanDesc pgdbscan;
	ScanKeyData key;
	HeapTuple	tup;
	Form_pg_database dbform;

	/*
	 * Because we grab AccessShareLock here, we can be sure that destroydb
	 * is not running in parallel with us (any more).
	 */
	pgdbrel = heap_openr(DatabaseRelationName, AccessShareLock);

	ScanKeyEntryInitialize(&key, 0, Anum_pg_database_datname,
						   F_NAMEEQ, NameGetDatum(name));

	pgdbscan = heap_beginscan(pgdbrel, 0, SnapshotNow, 1, &key);

	tup = heap_getnext(pgdbscan, 0);
	if (!HeapTupleIsValid(tup) ||
		tup->t_data->t_oid != MyDatabaseId)
	{
		/* OOPS */
		heap_close(pgdbrel, AccessShareLock);

		/*
		 * The only real problem I could have created is to load dirty
		 * buffers for the dead database into shared buffer cache; if I
		 * did, some other backend will eventually try to write them and
		 * die in mdblindwrt.  Flush any such pages to forestall trouble.
		 */
		DropBuffers(MyDatabaseId);
		/* Now I can commit hara-kiri with a clear conscience... */
		elog(FATAL, "Database \"%s\", OID %u, has disappeared from pg_database",
			 name, MyDatabaseId);
	}

	/*
	 * Also check that the database is currently allowing connections.
	 */
	dbform = (Form_pg_database) GETSTRUCT(tup);
	if (! dbform->datallowconn)
		elog(FATAL, "Database \"%s\" is not currently accepting connections",
			 name);

	/*
	 * OK, we're golden.  Only other to-do item is to save the MULTIBYTE
	 * encoding info out of the pg_database tuple.
	 */
#ifdef MULTIBYTE
	SetDatabaseEncoding(dbform->encoding);
#endif

	heap_endscan(pgdbscan);
	heap_close(pgdbrel, AccessShareLock);
}



/* --------------------------------
 *		InitCommunication
 *
 *		This routine initializes stuff needed for ipc, locking, etc.
 *		it should be called something more informative.
 * --------------------------------
 */
static void
InitCommunication()
{
	/* ----------------
	 *	initialize shared memory and semaphores appropriately.
	 * ----------------
	 */
	if (!IsUnderPostmaster)		/* postmaster already did this */
	{
		/* ----------------
		 *	we're running a postgres backend by itself with
		 *	no front end or postmaster.
		 * ----------------
		 */
		char	   *ipc_key;	/* value of environment variable */
		IPCKey		key;

		ipc_key = getenv("IPC_KEY");

		if (!PointerIsValid(ipc_key))
		{
			/* Normal standalone backend */
			key = PrivateIPCKey;
		}
		else
		{
			/* Allow standalone's IPC key to be set */
			key = atoi(ipc_key);
		}
		PostgresIpcKey = key;
		AttachSharedMemoryAndSemaphores(key);
	}
}



/* --------------------------------
 * InitPostgres
 *		Initialize POSTGRES.
 *
 * Note:
 *		Be very careful with the order of calls in the InitPostgres function.
 * --------------------------------
 */
int			lockingOff = 0;		/* backend -L switch */

/*
 */
void
InitPostgres(const char *dbname, const char *username)
{
	bool		bootstrap = IsBootstrapProcessingMode();

#ifndef XLOG
	if (!TransactionFlushEnabled())
		on_shmem_exit(FlushBufferPool, 0);
#endif

	SetDatabaseName(dbname);
	/* ----------------
	 *	initialize the database id used for system caches and lock tables
	 * ----------------
	 */
	if (bootstrap)
	{
		MyDatabaseId = TemplateDbOid;
#ifdef OLD_FILE_NAMING
		SetDatabasePath(ExpandDatabasePath(dbname));
#else
		SetDatabasePath(GetDatabasePath(MyDatabaseId));
#endif
		LockDisable(true);
	}
	else
	{
		char	   *fullpath,
					datpath[MAXPGPATH];

		/* Verify if DataDir is ok */
		if (access(DataDir, F_OK) == -1)
			elog(FATAL, "Database system not found. Data directory '%s' does not exist.",
				 DataDir);

		ValidatePgVersion(DataDir);

		/*-----------------
		 * Find oid and path of the database we're about to open. Since we're
		 * not yet up and running we have to use the hackish GetRawDatabaseInfo.
		 *
		 * OLD COMMENTS:
		 *		The database's oid forms half of the unique key for the system
		 *		caches and lock tables.  We therefore want it initialized before
		 *		we open any relations, since opening relations puts things in the
		 *		cache.	To get around this problem, this code opens and scans the
		 *		pg_database relation by hand.
		 */

		GetRawDatabaseInfo(dbname, &MyDatabaseId, datpath);

		if (!OidIsValid(MyDatabaseId))
			elog(FATAL,
				 "Database \"%s\" does not exist in the system catalog.",
				 dbname);

#ifdef OLD_FILE_NAMING
		fullpath = ExpandDatabasePath(datpath);
		if (!fullpath)
			elog(FATAL, "Database path could not be resolved.");
#else
		fullpath = GetDatabasePath(MyDatabaseId);
#endif

		/* Verify the database path */

		if (access(fullpath, F_OK) == -1)
			elog(FATAL, "Database \"%s\" does not exist. The data directory '%s' is missing.",
				 dbname, fullpath);

		ValidatePgVersion(fullpath);

		if (chdir(fullpath) == -1)
			elog(FATAL, "Unable to change directory to '%s': %s", fullpath, strerror(errno));

		SetDatabasePath(fullpath);
	}

	/*
	 * Code after this point assumes we are in the proper directory!
	 */

	/*
	 * Initialize the transaction system and the relation descriptor
	 * cache. Note we have to make certain the lock manager is off while
	 * we do this.
	 */
	AmiTransactionOverride(IsBootstrapProcessingMode());
	LockDisable(true);

	/*
	 * Part of the initialization processing done here sets a read lock on
	 * pg_log.	Since locking is disabled the set doesn't have intended
	 * effect of locking out writers, but this is ok, since we only lock
	 * it to examine AMI transaction status, and this is never written
	 * after initdb is done. -mer 15 June 1992
	 */
	RelationCacheInitialize();	/* pre-allocated reldescs created here */

	InitializeTransactionSystem();		/* pg_log,etc init/crash recovery
										 * here */

	LockDisable(false);

	/*
	 * Set up my per-backend PROC struct in shared memory.
	 */
	InitProcess(PostgresIpcKey);

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.  (Formerly this came before InitProcess, but now
	 * it must happen after, because it uses MyProc.)  Once I have done
	 * this, I am visible to other backends!
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	InitSharedInvalidationState();

	if (MyBackendId > MAXBACKENDS || MyBackendId <= 0)
		elog(FATAL, "cinit2: bad backend id %d", MyBackendId);

	/*
	 * Initialize the access methods. Does not touch files (?) - thomas
	 * 1997-11-01
	 */
	initam();

	/*
	 * Initialize all the system catalog caches.
	 *
	 * Does not touch files since all routines are builtins (?) - thomas
	 * 1997-11-01
	 */
	InitCatalogCache();

	/* start a new transaction here before access to db */
	if (!bootstrap)
		StartTransactionCommand();

	/* replace faked-up relcache entries with the real info */
	RelationCacheInitializePhase2();

	if (lockingOff)
		LockDisable(true);

	/*
	 * Set ourselves to the proper user id and figure out our postgres
	 * user id.
	 */
	if (bootstrap)
		SetSessionUserId(geteuid());
	else
		SetSessionUserIdFromUserName(username);

	setuid(geteuid());

	/*
	 * Unless we are bootstrapping, double-check that InitMyDatabaseInfo()
	 * got a correct result.  We can't do this until essentially all the
	 * infrastructure is up, so just do it at the end.
	 */
	if (!bootstrap)
		ReverifyMyDatabase(dbname);
}

void
BaseInit(void)
{
	/*
	 * Attach to shared memory and semaphores, and initialize our
	 * input/output/debugging file descriptors.
	 */
	InitCommunication();
	DebugFileOpen();
	smgrinit();

	EnablePortalManager();		/* memory for portal/transaction stuff */

	/* initialize the local buffer manager */
	InitLocalBuffer();

}
