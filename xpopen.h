/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */


#ifndef _XPOPEN_INCLUDED
#define _XPOPEN_INCLUDED

FILE *xpopen(char *cmd, char *mode);
int xpclose(FILE *f);

#endif /* _XPOPEN_INCLUDED */
	
