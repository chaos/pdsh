/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */


#include <conf.h>
 
#include <string.h>
#include <assert.h>
#include <unistd.h>	/* for R_OK, access() */
 
#include "dsh.h"
#include "err.h"
#include "list.h"
#include "xstring.h"	/* for xstrdup() */
#include "wcoll.h"

/*
 * Delete the first occurence of the specified host from the wcoll.
 *	wcoll (IN)	list of target nodes
 *	host (IN)	hostname to delete
 */
void del_wcoll(list_t wcoll, char *host)
{
	int wcoll_nitems = list_length(wcoll);
	int n;

	for (n = 0; n < wcoll_nitems; n++)
		if (strcmp(list_nth(wcoll, n), host) == 0) {
			list_del(wcoll, n);
			break;
		}
	assert(n < wcoll_nitems);
}

/* 
 * Read wcoll from specified file or from the specified FILE pointer.
 * (one of the arguments must be NULL).  
 *	file (IN)	name of wcoll file (or NULL)
 *	f (IN)		FILE pointer to wcoll file (or NULL)	
 *	RETURN		new list containing hostnames
 */
list_t read_wcoll(char *file, FILE *f)
{
	list_t new = list_new();
	list_t words;
	char buf[LINEBUFSIZE], *p, *word;
	FILE *fp;

	assert(f == NULL || file == NULL);

	if (f == NULL) {		/* read_wcoll("file", NULL) */
		if (access(file, R_OK) == -1)
			errx("%p: can't open %s for reading\n", file);
		fp = fopen(file, "r");
		if (!fp)
			errx("%p: can't open %s for reading\n", file);
	} else				/* read_wcoll(NULL, fp) */
		fp = f;

	while (fgets(buf, LINEBUFSIZE, fp) != NULL) {
		words = list_split(NULL, buf);
		if (list_length(words) > 0) {
			word = list_nth(words, 0);
			if (p = strchr(word, '#'))
				*p = '\0';
			p = word;
			xstrcln(&p, NULL);
			if (strlen(p) > 0)
				list_push(new, p);
		}
		list_free(&words);
	}
	if (f == NULL)
		fclose(fp);

	return new;
}

#if HAVE_GENDERS
list_t read_genders(char *attr, int iopt)
{
	FILE *f;
	list_t new = list_new();
	char cmd[LINEBUFSIZE];
	char buf[LINEBUFSIZE];
	char *p;

#if HAVE_SDRGETOBJECTS
	/* On SP's at LLNL, the genders names are the alternates */ 
	iopt = !iopt;
#endif

	sprintf(cmd, "%s -%sn %s", _PATH_NODEATTR, iopt ? "r" : "", attr);
	f = popen(cmd, "r");
	if (f == NULL)
		errx("%p: error running %s\n", _PATH_NODEATTR);
	while (fgets(buf, LINEBUFSIZE, f) != NULL) {
		p = buf;
		xstrcln(&p, NULL);
		list_push(new, p);
	}
	pclose(f);
	return new;
}
#endif

#if HAVE_SDRGETOBJECTS

static void sdr_getswitchname(char *switchName)
{
	FILE *f;
	list_t words;
	char cmd[LINEBUFSIZE];
	char buf[LINEBUFSIZE];
		
	sprintf(cmd, "%s -x Switch switch_number==1 switch_name",
	    _PATH_SDRGETOBJECTS);
	f = popen(cmd, "r");
	if (f == NULL)
       		errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
	while (fgets(buf, LINEBUFSIZE, f) != NULL) {
		words = list_split(NULL, buf);
		assert(list_length(words) == 1);
		strcpy(switchName, list_nth(words, 0));
		list_free(&words);
	}
	pclose(f);
}

/*
 * Query the SDR for switch_responds or host_responds for all nodes and return
 * the results in an array indexed by node number.
 *	Gopt (IN)	pass -G to SDRGetObjects
 *	nameType (IN)	either "switch_responds" or "host_responds"
 *	resp (OUT)	array of boolean, indexed by node number
 */
static void sdr_getresp(bool Gopt, char *nameType, bool resp[])
{
	int nn;
	FILE *f;
	list_t words;
	char cmd[LINEBUFSIZE];
	char buf[LINEBUFSIZE];
	char *attr = "host_responds";

	/* deal with Colony switch attribute name change */
	if (!strcmp(nameType, "switch_responds")) {
		sdr_getswitchname(buf);
		if (!strcmp(buf, "SP_Switch2"))
			attr = "switch_responds0";
		else
			attr = "switch_responds";
	}
		
	sprintf(cmd, "%s %s -x %s node_number %s", 
	    _PATH_SDRGETOBJECTS, Gopt ? "-G" : "", nameType, attr);
	f = popen(cmd, "r");
	if (f == NULL)
       		errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
	while (fgets(buf, LINEBUFSIZE, f) != NULL) {
		words = list_split(NULL, buf);
		assert(list_length(words) == 2);

		nn = atoi(list_nth(words, 0));
		assert(nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
		resp[nn] = (atoi(list_nth(words, 1)) == 1);
		list_free(&words);
	}
	pclose(f);
}

/*
 * Query the SDR for hostnames of all nodes and return the results in an 
 * array indexed by node number.
 *	Gopt (IN)	pass -G to SDRGetObjects
 *	nameType (IN)	either "initial_hostname" or "reliable_hostname"
 *	resp (OUT)	array of hostnames indexed by node number (heap cpy)
 */
static void sdr_getnames(bool Gopt, char *nameType, char *nodes[])
{
	int nn;
	FILE *f;
	list_t words;
	char cmd[LINEBUFSIZE];
	char buf[LINEBUFSIZE];

	sprintf(cmd, "%s %s -x Node node_number %s", 
	    _PATH_SDRGETOBJECTS, Gopt ? "-G" : "", nameType);
	f = popen(cmd, "r");
	if (f == NULL)
		errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
	while (fgets(buf, LINEBUFSIZE, f) != NULL) {
		words = list_split(NULL, buf);
		assert(list_length(words) == 2);

		nn = atoi(list_nth(words, 0));
		assert (nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
		nodes[nn] = xstrdup(list_nth(words, 1), NULL);
		list_free(&words);
	}
	pclose(f);
}

/*
 * Get the wcoll from the SDR.  
 *	Gopt (IN)	pass -G to SDRGetObjects
 *	altnames (IN)	ask for initial_hostname instead of reliable_hostname
 *	vopt (IN)	verify switch_responds/host_responds
 *	RETURN		new list containing hostnames
 */
list_t sdr_wcoll(bool Gopt, bool iopt, bool vopt)
{
	FILE *f;
	list_t words;
	list_t new;
	char cmd[LINEBUFSIZE];
	char buf[LINEBUFSIZE];
	char *inodes[MAX_SP_NODE_NUMBER + 1], *rnodes[MAX_SP_NODE_NUMBER + 1];
	bool sresp[MAX_SP_NODE_NUMBER + 1], hresp[MAX_SP_NODE_NUMBER + 1];
	char *p;
	int nn;
	char *sresp_attr;


	/*
	 * Build arrays of hostnames indexed by node number.  Array is size 
	 * MAX_SP_NODE_NUMBER, with NULL pointers set for unused nodes.
	 */
	for (nn = 0; nn <= MAX_SP_NODE_NUMBER; nn++) { 
		inodes[nn] = NULL;
		rnodes[nn] = NULL;
	}
	if (iopt)
		sdr_getnames(Gopt, "initial_hostname", inodes);
	else
		sdr_getnames(Gopt, "reliable_hostname", rnodes);

	/*
	 * Gather data needed to process -v.
	 */
	if (vopt) {
		if (iopt)
			sdr_getresp(Gopt, "switch_responds", sresp);
		sdr_getresp(Gopt, "host_responds", hresp);
	}
		
	/*
	 * Collect and return the nodes.  If -v was specified and a node is 
	 * not responding, substitute the alternate name; if that is not 
	 * responding, skip the node.
	 */
	new = list_new();
	for (nn = 0; nn <= MAX_SP_NODE_NUMBER; nn++) {
		if (inodes[nn] != NULL || rnodes[nn] != NULL) {
			if (vopt) { 			    /* initial_host */
				if (iopt && sresp[nn]) 
					list_push(new, inodes[nn]);
				else if (!iopt && hresp[nn])/* reliable_host */
					list_push(new, rnodes[nn]);
			} else {
				if (iopt)		    /* initial_host */
					list_push(new, inodes[nn]);
				else			    /* reliable_host */
					list_push(new, rnodes[nn]);
			}
			if (inodes[nn] != NULL)		    /* free heap cpys */
				xfree(&inodes[nn]);
			if (rnodes[nn] != NULL)
				xfree(&rnodes[nn]);
		}
	}

	return new;
}
#endif /* HAVE_SDRGETOBJECTS */
