/* 
 * $Id$ 
 *
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */ 

#ifndef _WCOLL_INCLUDED
#define _WCOLL_INCLUDED

#if     HAVE_CONFIG_H
#include "config.h"
#endif
 
#include "list.h"	/* for list_t */

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum {false, true} bool;
#endif

list_t read_wcoll(char *, FILE *);
void del_wcoll(list_t, char *);

#if	HAVE_SDR
list_t sdr_wcoll(bool, bool, bool);
#endif
#if 	HAVE_GENDERS
list_t read_genders(char *attr, int ropt);
#endif
#if 	HAVE_RMS
list_t rms_wcoll(void);
#endif

#endif
