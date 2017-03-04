/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2004-2005 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2016 Guillaume Bour <guillaume@bour.cc>
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
#include "../../lib/kcore/faked_msg.h"

#include "../../modules/tm/tm_load.h"

#include "keepalive.h"

struct tm_binds tmb;

static void ka_run_route(sip_msg_t *msg, str *uri, char *route);
static void ka_options_callback( struct cell *t, int type, struct tmcb_params *ps );


/*! \brief
 * Timer for checking probing destinations
 *
 * This timer is regularly fired.
 */
void ka_check_timer(unsigned int ticks, void* param)
{
	ka_dest_t *ka_dest;
	str ka_ping_method = str_init("OPTIONS");
	str ka_ping_from   = str_init("sip:dispatcher@localhost");
	str ka_outbound_proxy = {0, 0};
	uac_req_t uac_r;

	LM_DBG("ka check timer\n");

	for(ka_dest = ka_destinations_list->first; ka_dest != NULL; ka_dest = ka_dest->next) {
		LM_DBG("ka_check_timer dest:%.*s\n", ka_dest->uri.len, ka_dest->uri.s);

		/* Send ping using TM-Module.
		 * int request(str* m, str* ruri, str* to, str* from, str* h,
		 *		str* b, str *oburi,
		 *		transaction_cb cb, void* cbp); */
		set_uac_req(&uac_r, &ka_ping_method, 0, 0, 0,
				TMCB_LOCAL_COMPLETED, ka_options_callback,
				NULL);

		if (tmb.t_request(&uac_r,
					&ka_dest->uri,
					&ka_dest->uri,
					&ka_ping_from,
					&ka_outbound_proxy) < 0) {
			LM_ERR("unable to ping [%.*s]\n", ka_dest->uri.len, ka_dest->uri.s);
		}
	}

	return;
}

/*! \brief
 * Callback-Function for the OPTIONS-Request
 * This Function is called, as soon as the Transaction is finished
 * (e. g. a Response came in, the timeout was hit, ...)
 */
static void ka_options_callback( struct cell *t, int type,
		struct tmcb_params *ps )
{
	str uri = {0, 0};
	sip_msg_t *msg = NULL;

	uri.s = t->to.s + 5;
	uri.len = t->to.len - 8;
	LM_DBG("OPTIONS-Request was finished with code %d (to %.*s)\n",
			ps->code, uri.len, uri.s);

	// 408 Timeout
	if (ps->code == 408) {
		ka_run_route(msg, &uri, "keepalive:dst-down");
	} else {
		ka_run_route(msg, &uri, "keepalive:dst-up");
	}
}

/*
 * Execute kamailio script event routes
 *
 */
static void ka_run_route(sip_msg_t *msg, str *uri, char *route)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;

	if (route == NULL)
	{
		LM_ERR("bad route\n");
		return;
	}

	LM_DBG("ka_run_route event_route[%s]\n", route);

	rt = route_get(&event_rt, route);
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_DBG("route *%s* does not exist", route);
		return;
	}

	fmsg = msg;
	if (fmsg == NULL)
	{
		if (faked_msg_init() < 0)
		{
			LM_ERR("faked_msg_init() failed\n");
			return;
		}
		fmsg = faked_msg_next();
		fmsg->parsed_orig_ruri_ok = 0;
		fmsg->new_uri = *uri;
	}

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);
}

