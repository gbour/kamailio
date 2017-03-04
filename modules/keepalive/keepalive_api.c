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

#include "../../modules/tm/tm_load.h"

#include "keepalive.h"
#include "api.h"


/*
 * Regroup all exported functions in keepalive_api_t structure
 *
 */
int bind_keepalive(keepalive_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->add_destination = ka_add_dest;
	return 0;
}

/*
 * Add a new destination in keepalive pool
 */
int ka_add_dest(str uri, int flags) {
	LM_DBG("adding destination: %.*s\n", uri.len, uri.s);

	ka_dest_t *dest = (ka_dest_t *) shm_malloc(sizeof(ka_dest_t));
	if(dest == NULL) {
		LM_ERR("no more memory.\n");
		goto err;
	}
	memset(dest, 0, sizeof(ka_dest_t));

	// TODO: add sip: prefix if missing
	dest->uri.s = (char*) shm_malloc((uri.len + 1) * sizeof(char));
	if(dest->uri.s==NULL)
	{
		LM_ERR("no more memory!\n");
		shm_free(dest);

		goto err;
	}

	strncpy(dest->uri.s, uri.s, uri.len);
	dest->uri.s[uri.len] = '\0';
	dest->uri.len = uri.len;

	dest->flags = flags;

	dest->next                  = ka_destinations_list->first;
	ka_destinations_list->first = dest;

	return 0;

err:
	return -1;
}

/*
 * TODO
 */
int ka_rm_dest() {
	return -1;
}
