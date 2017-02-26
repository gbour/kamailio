/**
 * dispatcher module - load balancing
 *
 * Copyright (C) 2004-2005 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
 */

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../lib/kmi/mi.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../timer_proc.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "../../modules/tm/tm_load.h"

MODULE_VERSION

struct tm_binds tmb;

/** parameters */
static int mod_init(void);
static void destroy(void);
void ds_check_timer(unsigned int ticks, void* param);
static void ds_options_callback( struct cell *t, int type,
		struct tmcb_params *ps );

// no default value
str ka_peer = {NULL, 0};


static cmd_export_t cmds[]={
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"peer", PARAM_STR, &ka_peer},
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ 0, 0, 0, 0, 0}
};


/** module exports */
struct module_exports exports= {
	"keepalive",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	0           /* per-child init function */
};


/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("gateway= %.*s\n", ka_peer.len, ka_peer.s);

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (load_tm_api( &tmb ) == -1)
	{
		LM_ERR("could not load the TM-functions - please load tm module\n");
		return -1;
	}

	if(register_timer(ds_check_timer, NULL, 5) < 0)
		return -1;

	return 0;
}

/*! \brief
 * destroy function
 */
static void destroy(void)
{
}


/*! \brief
 * parses string to dispatcher dst flags set
 * returns <0 on failure or int with flag on success.
 */
int ds_parse_flags( char* flag_str, int flag_len )
{
	return 0;
}


/*! \brief
 * Timer for checking probing destinations
 *
 * This timer is regularly fired.
 */
void ds_check_timer(unsigned int ticks, void* param)
{
	str ds_ping_method = str_init("OPTIONS");
	str ds_ping_from   = str_init("sip:dispatcher@localhost");
	str ds_outbound_proxy = {0, 0};
	uac_req_t uac_r;

	LM_DBG("ds_check_timer %.*s\n", ka_peer.len, ka_peer.s);

	/* Send ping using TM-Module.
	 * int request(str* m, str* ruri, str* to, str* from, str* h,
	 *		str* b, str *oburi,
	 *		transaction_cb cb, void* cbp); */
	set_uac_req(&uac_r, &ds_ping_method, 0, 0, 0,
			TMCB_LOCAL_COMPLETED, ds_options_callback,
			NULL);

	if (tmb.t_request(&uac_r,
				&ka_peer,
				&ka_peer,
				&ds_ping_from,
				&ds_outbound_proxy) < 0) {
		LM_ERR("unable to ping [%.*s]\n", ka_peer.len, ka_peer.s);
	}

	return;
}

/*! \brief
 * Callback-Function for the OPTIONS-Request
 * This Function is called, as soon as the Transaction is finished
 * (e. g. a Response came in, the timeout was hit, ...)
 */
static void ds_options_callback( struct cell *t, int type,
		struct tmcb_params *ps )
{
	LM_DBG("options callback\n");
}
