/* 
 * $Id$ 
 *
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _ERR_INCLUDED
#define _ERR_INCLUDED

void err_init(char *);
void err(char *, ...);
void out(char *, ...);
void errx(char *, ...);
void err_cleanup(void);

#endif
