/*-------------------------------------------------------------------------
 *
 * pgarch.h
 *	  Exports from postmaster/pgarch.c.
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PGARCH_H
#define _PGARCH_H

/* ----------
 * Functions called from postmaster
 * ----------
 */
extern int	pgarch_start(void);

#ifdef EXEC_BACKEND
extern void PgArchiverMain(int argc, char *argv[]);
#endif

#endif   /* _PGARCH_H */
