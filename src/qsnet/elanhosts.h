
#ifndef _ELANHOSTS_H
#define _ELANHOSTS_H

typedef enum {
	ELANHOST_EIP,
	ELANHOST_ETH,
	ELANHOST_OTHER
} elanhost_type_t;

/*  Opaque type which holds the elanhost configuration
 */
typedef struct elanhost_config * elanhost_config_t;


/*
 *  Functions
 */

/*
 * Create an empty Elanhost config object
 */
elanhost_config_t elanhost_config_create(void);

/*
 *  Read elanhosts configuration from `file'  
 *    (Default /etc/elanhosts)
 *
 *  Config file format is as follows:
 *
 *    Type  ElanIDs  Hostnames
 *
 *  The "Type" field corresponds
 *    ethernet interface, or "other" for anything else. ("eth" and
 *    "other" are equivalent at this time)
 *
 *  The "ElanIDs" field consists of a list of one or more ElanIDs in
 *    the form "[i-j,n-m,..]" or just "N" for a single ElanID.
 *
 *  The "Hostnames" field consists of the hostnames which correspond
 *    to the ElanIDs. If the hostnames have a numeric suffix a bracketed
 *    hostlist is allowed (see hostlist.[ch]) 
 *
 *  For Example:
 *
 *    Type  ElanIDs  Hostnames
 *    eip   [0-10]   host[0-10]
 *    eth   [0-10]   ehost[0-10]
 *    eth   [0,1]    host0-eth1,host1-eth1
 *
 *  Returns 0 on succes, -1 for failure.
 *
 */
int elanhost_config_read(elanhost_config_t ec, const char *filename);


/*
 *  Destroy an elanhost configuration object.
 */
void elanhost_config_destroy(elanhost_config_t conf);


/*
 *  Given a hostname, return the corresponding ElanID
 *
 *  Returns the ElanId on success, -1 if no host matching "hostname"
 *    was found in the configuration.
 *
 */
int elanhost_host2elanid(elanhost_config_t ec, char *host);


/*
 *  Given an ElanId and adapter type, return the first matching hostname
 *    from the configuration.
 */
char *elanhost_elanid2host(elanhost_config_t ec, 
		           elanhost_type_t type, int elanid);


/* 
 *  Returns the max ElanID from the configuration
 */
int elanhost_config_maxid(elanhost_config_t ec);


/*
 *  Returns the last error string generated for the elan config obj `ec'
 */
const char *elanhost_config_err(elanhost_config_t ec);


#endif
