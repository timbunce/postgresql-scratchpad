/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright 2000 by PostgreSQL Global Development Group
 *
 * $Header$
 */
#ifndef MAINLOOP_H
#define MAINLOOP_H

#include "postgres.h"
#include <stdio.h>
#ifndef WIN32
#include <setjmp.h>

extern sigjmp_buf main_loop_jmp;

#endif

int			MainLoop(FILE *source);

#endif	 /* MAINLOOP_H */
