/*-------------------------------------------------------------------------
 *
 * ipc.h
 *	  POSTGRES inter-process communication definitions.
 *
 *
 * Copyright (c) 1994, Regents of the University of California
 *
 * $Id$
 *
 * NOTES
 *	  This file is very architecture-specific.	This stuff should actually
 *	  be factored into the port/ directories.
 *
 * Some files that would normally need to include only sys/ipc.h must
 * instead included this file because on Ultrix, sys/ipc.h is not designed
 * to be included multiple times.  This file (by virtue of the ifndef IPC_H)
 * is.
 *-------------------------------------------------------------------------
 */
#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/ipc.h>			/* For IPC_PRIVATE */

#include <config.h>

#ifndef HAVE_UNION_SEMUN
union semun
{
	int			val;
	struct semid_ds *buf;
	unsigned short *array;
};

#endif

typedef uint16 SystemPortAddress;

/* semaphore definitions */

#define IPCProtection	(0600)	/* access/modify by user only */

#define IPC_NMAXSEM		25		/* maximum number of semaphores */
#define IpcSemaphoreDefaultStartValue	255
#define IpcSharedLock									(-1)
#define IpcExclusiveLock						  (-255)

#define IpcUnknownStatus		(-1)
#define IpcInvalidArgument		(-2)
#define IpcSemIdExist			(-3)
#define IpcSemIdNotExist		(-4)

typedef uint32 IpcSemaphoreKey; /* semaphore key */
typedef int IpcSemaphoreId;

/* shared memory definitions */

#define IpcMemCreationFailed	(-1)
#define IpcMemIdGetFailed		(-2)
#define IpcMemAttachFailed		0

typedef uint32 IPCKey;

#define PrivateIPCKey	IPC_PRIVATE
#define DefaultIPCKey	17317

typedef uint32 IpcMemoryKey;	/* shared memory key */
typedef int IpcMemoryId;


/* ipc.c */
extern void proc_exit(int code);
extern void shmem_exit(int code);
extern int	on_shmem_exit(void (*function) (), caddr_t arg);
extern int	on_proc_exit(void (*function) (), caddr_t arg);
extern void on_exit_reset(void);

extern IpcSemaphoreId IpcSemaphoreCreate(IpcSemaphoreKey semKey,
				   int semNum, int permission, int semStartValue,
				   int removeOnExit, int *status);
extern void IpcSemaphoreKill(IpcSemaphoreKey key);
extern void IpcSemaphoreLock(IpcSemaphoreId semId, int sem, int lock);
extern void IpcSemaphoreUnlock(IpcSemaphoreId semId, int sem, int lock);
extern int	IpcSemaphoreGetCount(IpcSemaphoreId semId, int sem);
extern int	IpcSemaphoreGetValue(IpcSemaphoreId semId, int sem);
extern IpcMemoryId IpcMemoryCreate(IpcMemoryKey memKey, uint32 size,
				int permission);
extern IpcMemoryId IpcMemoryIdGet(IpcMemoryKey memKey, uint32 size);
extern char *IpcMemoryAttach(IpcMemoryId memId);
extern void IpcMemoryKill(IpcMemoryKey memKey);
extern void CreateAndInitSLockMemory(IPCKey key);
extern void AttachSLockMemory(IPCKey key);


#ifdef HAS_TEST_AND_SET

#define NOLOCK			0
#define SHAREDLOCK		1
#define EXCLUSIVELOCK	2

typedef enum _LockId_
{
	BUFMGRLOCKID,
	LOCKLOCKID,
	OIDGENLOCKID,
	SHMEMLOCKID,
	SHMEMINDEXLOCKID,
	LOCKMGRLOCKID,
	SINVALLOCKID,

#ifdef STABLE_MEMORY_STORAGE
	MMCACHELOCKID,
#endif

	PROCSTRUCTLOCKID,
	FIRSTFREELOCKID
} _LockId_;

#define MAX_SPINS		FIRSTFREELOCKID

typedef struct slock
{
	slock_t		locklock;
	unsigned char flag;
	short		nshlocks;
	slock_t		shlock;
	slock_t		exlock;
	slock_t		comlock;
	struct slock *next;
} SLock;

#else							/* HAS_TEST_AND_SET */

typedef enum _LockId_
{
	SHMEMLOCKID,
	SHMEMINDEXLOCKID,
	BUFMGRLOCKID,
	LOCKMGRLOCKID,
	SINVALLOCKID,

#ifdef STABLE_MEMORY_STORAGE
	MMCACHELOCKID,
#endif

	PROCSTRUCTLOCKID,
	OIDGENLOCKID,
	FIRSTFREELOCKID
} _LockId_;

#define MAX_SPINS		FIRSTFREELOCKID

#endif	 /* HAS_TEST_AND_SET */

/*
 * the following are originally in ipci.h but the prototypes have circular
 * dependencies and most files include both ipci.h and ipc.h anyway, hence
 * combined.
 *
 */

/*
 * Note:
 *		These must not hash to DefaultIPCKey or PrivateIPCKey.
 */
#define SystemPortAddressGetIPCKey(address) \
		(28597 * (address) + 17491)

/*
 * these keys are originally numbered from 1 to 12 consecutively but not
 * all are used. The unused ones are removed.			- ay 4/95.
 */
#define IPCKeyGetBufferMemoryKey(key) \
		((key == PrivateIPCKey) ? key : 1 + (key))

#define IPCKeyGetSIBufferMemoryBlock(key) \
		((key == PrivateIPCKey) ? key : 7 + (key))

#define IPCKeyGetSLockSharedMemoryKey(key) \
		((key == PrivateIPCKey) ? key : 10 + (key))

#define IPCKeyGetSpinLockSemaphoreKey(key) \
		((key == PrivateIPCKey) ? key : 11 + (key))
#define IPCKeyGetWaitIOSemaphoreKey(key) \
		((key == PrivateIPCKey) ? key : 12 + (key))
#define IPCKeyGetWaitCLSemaphoreKey(key) \
		((key == PrivateIPCKey) ? key : 13 + (key))

/* --------------------------
 * NOTE: This macro must always give the highest numbered key as every backend
 * process forked off by the postmaster will be trying to acquire a semaphore
 * with a unique key value starting at key+14 and incrementing up.	Each
 * backend uses the current key value then increments it by one.
 * --------------------------
 */
#define IPCGetProcessSemaphoreInitKey(key) \
		((key == PrivateIPCKey) ? key : 14 + (key))

/* ipci.c */
extern IPCKey SystemPortAddressCreateIPCKey(SystemPortAddress address);
extern void CreateSharedMemoryAndSemaphores(IPCKey key, int maxBackends);
extern void AttachSharedMemoryAndSemaphores(IPCKey key);

#endif	 /* IPC_H */
