/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */ 

#ifndef _WCOLL_INCLUDED
#define _WCOLL_INCLUDED

#include <conf.h>
 
#include "list.h"	/* for list_t */

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum {false, true} bool;
#endif

list_t read_wcoll(char *, FILE *);
void del_wcoll(list_t, char *);

#ifdef HAVE_SDRGETOBJECTS
list_t sdr_wcoll(bool, bool, bool);
#endif
#ifdef HAVE_GENDERS
list_t read_genders(char *attr, int ropt);
#endif

#endif
