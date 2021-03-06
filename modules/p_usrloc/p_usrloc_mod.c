/*
 * $Id$
 *
 * Usrloc module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-01-27 timer activity printing #ifdef-ed to EXTRA_DEBUG (jiri)
 * 2003-03-11 New module interface (janakj)
 * 2003-03-12 added replication and state columns (nils)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-21 failed fifo init stops init process (jiri)
 * 2004-03-17 generic callbacks added (bogdan)
 * 2004-06-07 updated to the new DB api (andrei)
 * 2011-01-03 extended module definition for partitioned userlocking (marius)
 */

/*! \file
 *  \brief USRLOC - Usrloc module interface
 *  \ingroup usrloc
 *
 * - Module \ref usrloc
 */

/*!
 * \defgroup usrloc Usrloc :: User location module
 * \brief User location module
 *
 * The module keeps a user location table and provides access
 * to the table to other modules. The module exports no functions
 * that could be used directly from scripts, all access is done
 * over a API. A main user of this API is the registrar module.
 * \see registrar
 */

#include <stdio.h>
#include "p_usrloc_mod.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../rpc_lookup.h"
#include "../../timer_proc.h"
#include "../../globals.h"   /* is_main */
#include "../../ut.h"        /* str_init */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "urecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_mi.h"
#include "../usrloc/ul_callback.h"
#include "ul_db_api.h"
#include "ul_db_watch.h"
#include "ul_check.h"
#include "ul_db.h"
#include "ul_db_layer.h"
#include "dlist.h"

MODULE_VERSION

#define RUID_COL       "ruid"
#define USER_COL       "username"
#define DOMAIN_COL     "domain"
#define CONTACT_COL    "contact"
#define EXPIRES_COL    "expires"
#define Q_COL          "q"
#define CALLID_COL     "callid"
#define CSEQ_COL       "cseq"
#define FLAGS_COL      "flags"
#define CFLAGS_COL 	"cflags"
#define USER_AGENT_COL "user_agent"
#define RECEIVED_COL   "received"
#define PATH_COL       "path"
#define SOCK_COL       "socket"
#define METHODS_COL    "methods"
#define INSTANCE_COL   "instance"
#define REG_ID_COL     "reg_id"
#define LAST_MOD_COL   "last_modified"

static int mod_init(void);                          /*!< Module initialization function */
static void destroy(void);                          /*!< Module destroy function */
static int child_init(int rank);                    /*!< Per-child init function */
static int mi_child_init(void);
static int mi_child_loc_nr_init(void);
extern int bind_usrloc(usrloc_api_t* api);
extern int ul_locks_no;
/*
 * Module parameters and their default values
 */

/**
 * @var params
 * defines the parameters which can be set in the Kamailio config file
 * is stored. Only used when @see use_domain is set to 1
 * @param write_db_url Url to the database where the key and database information is 
 * stored and where errors are reported to. Only used when @see write_on_db is active.
 * @param read_db_url Url to the database where the key and database information is 
 * stored.
 * @param reg_db_table the name of the table containing the information about the 
 * partitioned databases.
 * @param id_column name of the column containing the id mapping to a key.
 * @param num_column name of the column containing the number of the entry.
 * @param url_column name of the column containing the URL to the database.
 * @param status_column name of the column containing the status of the database. 
 * (1=ON, 2=OFF)
 * @param failover_time_column name of the column containing the time whem the 
 * database's status changed or a spare has been activated.
 * @param spare_flag_column name of the column containing the information if an entry
 * works as spare for broken dbs (0=no spare, 1=spare)
 * @param error_column name of the column containing the errors which occured on 
 * the database.
 * @param risk_group_column name of the column containing the databases risk group
 * Only used when spare databases are used.
 * @param expire_time specifies the expire time of contacts
 * @param db_err_threshold specifies the amount of errors when at which a db 
 * gets deactivated
 * @param failover_level defines if the module shall search for spares or just
 * turnoff a broken db
 * @param db_retry_interval defines in which intervals the module shall try to 
 * reconnect to a deactivated database
 * @param write_on_db defines if the module has write access on the databases or not
 * @param alg_location defines the algorithm for the location matching - based on crc32 for  now
 */

str ruid_col        = str_init(RUID_COL); 		/*!< Name of column containing record unique id */
str user_col        = str_init(USER_COL); 		/*!< Name of column containing usernames */
str domain_col      = str_init(DOMAIN_COL); 		/*!< Name of column containing domains */
str contact_col     = str_init(CONTACT_COL);		/*!< Name of column containing contact addresses */
str expires_col     = str_init(EXPIRES_COL);		/*!< Name of column containing expires values */
str q_col           = str_init(Q_COL);			/*!< Name of column containing q values */
str callid_col      = str_init(CALLID_COL);		/*!< Name of column containing callid string */
str cseq_col        = str_init(CSEQ_COL);		/*!< Name of column containing cseq values */
str flags_col       = str_init(FLAGS_COL);		/*!< Name of column containing internal flags */
str cflags_col       = str_init(CFLAGS_COL);
str user_agent_col  = str_init(USER_AGENT_COL);		/*!< Name of column containing user agent string */
str received_col    = str_init(RECEIVED_COL);		/*!< Name of column containing transport info of REGISTER */
str path_col        = str_init(PATH_COL);		/*!< Name of column containing the Path header */
str sock_col        = str_init(SOCK_COL);		/*!< Name of column containing the received socket */
str methods_col     = str_init(METHODS_COL);		/*!< Name of column containing the supported methods */
str instance_col    = str_init(INSTANCE_COL);	/*!< Name of column containing the SIP instance value */
str reg_id_col      = str_init(REG_ID_COL);		/*!< Name of column containing the reg-id value */
str last_mod_col     = str_init(LAST_MOD_COL);		/*!< Name of column containing the last modified date */
int db_mode         = 3;				/*!< Database sync scheme:  1-write through, 2-write back, 3-only db */
int use_domain      = 0;				/*!< Whether usrloc should use domain part of aor */
int desc_time_order = 0;				/*!< By default do not enable timestamp ordering */

int ul_fetch_rows = 2000;				/*!< number of rows to fetch from result */
int ul_hash_size = 9;
str write_db_url         = {DEFAULT_DB_URL, DEFAULT_DB_URL_LEN};
str read_db_url          = {DEFAULT_DB_URL, DEFAULT_DB_URL_LEN};
str reg_table            = {REG_TABLE, sizeof(REG_TABLE) -1};
str id_col               = {ID_COL, sizeof(ID_COL) - 1};
str url_col              = {URL_COL, sizeof(URL_COL) - 1};
str num_col              = {NUM_COL, sizeof(NUM_COL) - 1};
str status_col           = {STATUS_COL, sizeof(STATUS_COL) - 1};
str failover_time_col    = {FAILOVER_T_COL, sizeof(FAILOVER_T_COL) - 1};
str spare_col            = {SPARE_COL, sizeof(SPARE_COL) - 1};
str error_col            = {ERROR_COL, sizeof(ERROR_COL) - 1};
str risk_group_col       = {RISK_GROUP_COL, sizeof(RISK_GROUP_COL) - 1};
int expire_time          = DEFAULT_EXPIRE;
int db_error_threshold   = DEFAULT_ERR_THRESHOLD;
int failover_level       = DEFAULT_FAILOVER_LEVEL;
int retry_interval       = DB_RETRY;
int policy               = DB_DEFAULT_POLICY;
int db_write             = 0;
int db_master_write      = 0;
int alg_location         = 0;

int db_use_transactions  = 0;
str db_transaction_level = {DB_DEFAULT_TRANSACTION_LEVEL, sizeof(DB_DEFAULT_TRANSACTION_LEVEL) -1};
char * isolation_level;
int connection_expires   = DB_DEFAULT_CONNECTION_EXPIRES;
int max_loc_nr  = 0 ;


/* flags */
unsigned int nat_bflag = (unsigned int)-1;
unsigned int init_flag = 0;

str default_db_url    = str_init(DEFAULT_DB_URL);
str default_db_type   = str_init(DEFAULT_DB_TYPE);
str domain_db         = str_init(DEFAULT_DOMAIN_DB);
int default_dbt       = 0;
int expire            = 0;


/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ul_bind_usrloc",        (cmd_function)bind_usrloc,        1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters 
 */
static param_export_t params[] = {
	{"ruid_column",         STR_PARAM, &ruid_col.s      },
	{"user_column",       STR_PARAM, &user_col.s      },
	{"domain_column",     STR_PARAM, &domain_col.s    },
	{"contact_column",    STR_PARAM, &contact_col.s   },
	{"expires_column",    STR_PARAM, &expires_col.s   },
	{"q_column",          STR_PARAM, &q_col.s         },
	{"callid_column",     STR_PARAM, &callid_col.s    },
	{"cseq_column",       STR_PARAM, &cseq_col.s      },
	{"flags_column",      STR_PARAM, &flags_col.s     },
	{"cflags_column",     STR_PARAM, &cflags_col.s    },
	{"db_mode",           INT_PARAM, &db_mode         },
	{"use_domain",        INT_PARAM, &use_domain      },
	{"desc_time_order",   INT_PARAM, &desc_time_order },
	{"user_agent_column", STR_PARAM, &user_agent_col.s},
	{"received_column",   STR_PARAM, &received_col.s  },
	{"path_column",       STR_PARAM, &path_col.s      },
	{"socket_column",     STR_PARAM, &sock_col.s      },
	{"methods_column",    STR_PARAM, &methods_col.s   },
	{"matching_mode",     INT_PARAM, &matching_mode   },
	{"cseq_delay",        INT_PARAM, &cseq_delay      },
	{"fetch_rows",        INT_PARAM, &ul_fetch_rows   },
	{"hash_size",         INT_PARAM, &ul_hash_size    },
	{"nat_bflag",         INT_PARAM, &nat_bflag       },
	{"default_db_url",    STR_PARAM, &default_db_url.s    },
	{"default_db_type",   STR_PARAM, &default_db_type.s   },
	{"domain_db",         STR_PARAM, &domain_db.s         },
	{"instance_column",      STR_PARAM, &instance_col.s  	 },
	{"reg_id_column",      	 STR_PARAM, &reg_id_col.s        },
	{"write_db_url",         STR_PARAM, &write_db_url.s      },
	{"read_db_url",          STR_PARAM, &read_db_url.s       },
	{"reg_db_table",         STR_PARAM, &reg_table.s         },
	{"id_column",            STR_PARAM, &id_col.s            },
	{"num_column",           STR_PARAM, &num_col.s           },
	{"url_column",           STR_PARAM, &url_col.s           },
	{"status_column",        STR_PARAM, &status_col.s        },
	{"failover_time_column", STR_PARAM, &failover_time_col.s },
	{"spare_flag_column",    STR_PARAM, &spare_col.s         },
	{"error_column",         STR_PARAM, &error_col.s         },
	{"risk_group_column",    STR_PARAM, &risk_group_col.s    },
	{"expire_time",          INT_PARAM, &expire_time         },
	{"db_err_threshold",     INT_PARAM, &db_error_threshold  },
	{"failover_level",       INT_PARAM, &failover_level      },
	{"db_retry_interval",    INT_PARAM, &retry_interval      },
	{"db_use_transactions",  INT_PARAM, &db_use_transactions },
	{"db_transaction_level", INT_PARAM, &db_transaction_level},
	{"write_on_db",          INT_PARAM, &db_write            },
	{"write_on_master_db",   INT_PARAM, &db_master_write     },
	{"connection_expires",   INT_PARAM, &connection_expires  },
	{"alg_location",         INT_PARAM, &alg_location },
	{0, 0, 0}
};


stat_export_t mod_stats[] = {
	{"registered_users" ,  STAT_IS_FUNC, (stat_var**)get_number_of_users  },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ MI_USRLOC_RM,           mi_usrloc_rm_aor,       0,                 0,
				mi_child_init },
	{ MI_USRLOC_RM_CONTACT,   mi_usrloc_rm_contact,   0,                 0,
				mi_child_init },
	{ MI_USRLOC_DUMP,         mi_usrloc_dump,         0,                 0,
				0             },
	{ MI_USRLOC_FLUSH,        mi_usrloc_flush,        MI_NO_INPUT_FLAG,  0,
				mi_child_init },
	{ MI_USRLOC_ADD,          mi_usrloc_add,          0,                 0,
				mi_child_init },
	{ MI_USRLOC_SHOW_CONTACT, mi_usrloc_show_contact, 0,                 0,
				mi_child_init },
	{ "loc_refresh", mi_ul_db_refresh, MI_NO_INPUT_FLAG,  0, mi_child_init },
	{ "loc_nr_refresh", mi_loc_nr_refresh, MI_NO_INPUT_FLAG,  0, mi_child_loc_nr_init },
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"p_usrloc",
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< Exported functions */
	params,     /*!< Export parameters */
	mod_stats,  /*!< exported statistics */
	mi_cmds,    /*!< exported MI functions */
	0,          /*!< exported pseudo-variables */
	0,          /*!< extra processes */
	mod_init,   /*!< Module initialization function */
	0,          /*!< Response function */
	destroy,    /*!< Destroy function */
	child_init  /*!< Child initialization function */
};


/*! \brief
 * Module initialization function
 */
static int mod_init(void)
{
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif
	
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}


	/* Compute the lengths of string parameters */
	ruid_col.len = strlen(ruid_col.s);
	user_col.len = strlen(user_col.s);
	domain_col.len = strlen(domain_col.s);
	contact_col.len = strlen(contact_col.s);
	expires_col.len = strlen(expires_col.s);
	q_col.len = strlen(q_col.s);
	callid_col.len = strlen(callid_col.s);
	cseq_col.len = strlen(cseq_col.s);
	flags_col.len = strlen(flags_col.s);
	cflags_col.len = strlen(cflags_col.s);
	user_agent_col.len = strlen(user_agent_col.s);
	received_col.len = strlen(received_col.s);
	path_col.len = strlen(path_col.s);
	sock_col.len = strlen(sock_col.s);
	methods_col.len = strlen(methods_col.s);
	instance_col.len = strlen(instance_col.s);
	reg_id_col.len = strlen(reg_id_col.s);
	last_mod_col.len = strlen(last_mod_col.s);
	
	write_db_url.len = strlen (write_db_url.s);
	read_db_url.len = strlen (read_db_url.s);
	reg_table.len = strlen(reg_table.s);
	id_col.len = strlen(id_col.s);
	url_col.len = strlen(url_col.s);
	num_col.len = strlen(num_col.s);
	status_col.len = strlen(status_col.s);
	failover_time_col.len = strlen(failover_time_col.s);
	spare_col.len = strlen(spare_col.s);
	error_col.len = strlen(error_col.s);
	risk_group_col.len = strlen(risk_group_col.s);
	db_transaction_level.len = strlen(db_transaction_level.s);

	
	if(ul_hash_size<=1)
		ul_hash_size = 512;
	else
		ul_hash_size = 1<<ul_hash_size;
	ul_locks_no = ul_hash_size;

	/* check matching mode */
	switch (matching_mode) {
		case CONTACT_ONLY:
		case CONTACT_CALLID:
		case CONTACT_PATH:
			break;
		default:
			LM_ERR("invalid matching mode %d\n", matching_mode);
	}

	if(ul_init_locks()!=0)
	{
		LM_ERR("locks array initialization failed\n");
		return -1;
	}

	/* init the callbacks list */
	if ( init_ulcb_list() < 0) {
		LM_ERR("usrloc/callbacks initialization failed\n");
		return -1;
	}

	if (db_mode != DB_ONLY) {
		LM_ERR("DB_ONLY is the only mode possible for partitioned usrloc. Please set db_mode to 3");
		return  -1;
	}

	/* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		if(!default_db_url.s || !strlen(default_db_url.s)){
			LM_ERR("must set default_db_url parameter\n");
			return -1;
		}
		default_db_url.len = strlen (default_db_url.s);

		domain_db.len = strlen(domain_db.s);
		default_db_type.len = strlen(default_db_type.s);
		if(strcmp(DB_TYPE_CLUSTER_STR, default_db_type.s) == 0){
			default_dbt = DB_TYPE_CLUSTER;
		} else {
			default_dbt = DB_TYPE_SINGLE;
		}

		if (ul_db_layer_init() < 0) {
			return -1;
		}

		if(ul_fetch_rows<=0) {
			LM_ERR("invalid fetch_rows number '%d'\n", ul_fetch_rows);
			return -1;
		}
	}

	if (nat_bflag==(unsigned int)-1) {
		nat_bflag = 0;
	} else if ( nat_bflag>=8*sizeof(nat_bflag) ) {
		LM_ERR("bflag index (%d) too big!\n", nat_bflag);
		return -1;
	} else {
		nat_bflag = 1<<nat_bflag;
	}

	init_flag = 1;
	
	if((isolation_level = pkg_malloc(strlen("SET TRANSACTION ISOLATION LEVEL ") + db_transaction_level.len + 1)) == NULL){
		LM_ERR("couldn't allocate private memory.\n");
		return -1;
	}
	sprintf(isolation_level, "SET TRANSACTION ISOLATION LEVEL %s", db_transaction_level.s);
	if(init_list() < 0) {
		LM_ERR("could not init check list.\n");
		return -1;
	}
	if(ul_db_init() < 0){
		LM_ERR("could not initialise databases.\n");
		return -1;
	}
	if(ul_db_watch_init() < 0){
		LM_ERR("could not init database watch environment.\n");
		return -1;
	}
	if(db_master_write){
		/* register extra dummy timer to be created in init_db_check() */
		register_dummy_timers(1);
	}
	return 0;
}


static int child_init(int _rank)
{
	if(_rank==PROC_INIT) {
		if(init_db_check() < 0){
				LM_ERR("could not initialise database check.\n");
			return -1;
		}
		return 0;
	}
	if(ul_db_child_init() < 0){
		LM_ERR("could not initialise databases.\n");
		return -1;
	}
	

	return 0;
}


/* */
static int mi_child_init(void)
{

	return ul_db_child_init();
}


/*! \brief
 * Module destroy function
 */
static void destroy(void)
{
	/* we need to sync DB in order to flush the cache */
	ul_unlock_locks();

	free_all_udomains();
	ul_destroy_locks();
	/* free callbacks list */
	destroy_ulcb_list();
	
	ul_db_shutdown();
	destroy_list();
	ul_db_watch_destroy();
	
}


static int mi_child_loc_nr_init(void)
{
	if(ul_db_child_locnr_init() < 0){
		LM_ERR("could not retrive location number from database. Try to reinitialize the db handles\n");
		return -1;
	}
	return 0;
}


struct mi_root*  mi_ul_db_refresh(struct mi_root* cmd_tree, void* param) {
	int ret;
	ret = set_must_refresh();
	
	LM_INFO("sp-ul_db location databases were refreshed (%i elements).\n", ret);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}


struct mi_root*  mi_loc_nr_refresh(struct mi_root* cmd_tree, void* param) {
	/* this function does nothing, all work is done per each child in the mi_child_loc_nr_init function */
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}
