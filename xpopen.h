/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */


#ifndef _HAVE_XPOPEN
#define _HAVE_XPOPEN

FILE *xpopen(char *cmd, char *mode);
int xpclose(FILE *f);

#endif /* _HAVE_XPOPEN */
	
