/*-------------------------------------------------------------------------
 *
 * datetime.h--
 *    Definitions for the datetime
 *
 *
 * Copyright (c) 1994, Regents of the University of California
 *
 * $Id$
 *
 *-------------------------------------------------------------------------
 */
#ifndef DATETIME_H
#define DATETIME_H

/* these things look like structs, but we pass them by value so be careful
   For example, passing an int -> DateADT is not portable! */
typedef struct DateADT {
    char	day;
    char	month;
    short	year;
} DateADT;

typedef struct TimeADT {
    short	hr;
    short	min;
    float	sec;
} TimeADT;

#endif /* DATETIME_H */
