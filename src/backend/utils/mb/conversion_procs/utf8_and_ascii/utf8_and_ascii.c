/*-------------------------------------------------------------------------
 *
 *	  ASCII <--> UTF-8
 *
 * Portions Copyright (c) 1996-2002, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $Header$
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "mb/pg_wchar.h"

PG_FUNCTION_INFO_V1(ascii_to_utf8)
PG_FUNCTION_INFO_V1(utf8_to_ascii)

extern Datum ascii_to_utf8(PG_FUNCTION_ARGS);
extern Datum utf8_to_ascii(PG_FUNCTION_ARGS);

/* ----------
 * conv_proc(
 *		INTEGER,	-- source encoding id
 *		INTEGER,	-- destination encoding id
 *		OPAQUE,		-- source string (null terminated C string)
 *		OPAQUE,		-- destination string (null terminated C string)
 *		INTEGER		-- source string length
 * ) returns INTEGER;	-- dummy. returns nothing, actually.
 * ----------
 */

Datum
ascii_to_utf8(PG_FUNCTION_ARGS)
{
	unsigned char *src = PG_GETARG_CSTRING(2);
	unsigned char *dest = PG_GETARG_CSTRING(3);
	int len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_SQL_ASCII);
	Assert(PG_GETARG_INT32(1) == PG_UTF8);
	Assert(len > 0);

	pg_ascii2mic(src, dest, len);

	PG_RETURN_INT32(0);
}

Datum
utf8_to_ascii(PG_FUNCTION_ARGS)
{
	unsigned char *src = PG_GETARG_CSTRING(2);
	unsigned char *dest = PG_GETARG_CSTRING(3);
	int len = PG_GETARG_INT32(4);

	Assert(PG_GETARG_INT32(0) == PG_UTF8);
	Assert(PG_GETARG_INT32(1) == PG_SQL_ASCII);
	Assert(len > 0);

	pg_mic2ascii(src, dest, len);

	PG_RETURN_INT32(0);
}
