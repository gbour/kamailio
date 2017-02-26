/**
 * keepalive module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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
 * \brief Keepalive :: Keepalive
 */

#ifndef _KEEPALIVE_H_
#define _KEEPALIVE_H_

#define KA_INACTIVE_DST		1  /*!< inactive destination */
#define KA_TRYING_DST		2  /*!< temporary trying destination */
#define KA_DISABLED_DST		4  /*!< admin disabled destination */
#define KA_PROBING_DST		8  /*!< checking destination */
#define KA_STATES_ALL		15  /*!< all bits for the states of destination */

#define ds_skip_dst(flags)	((flags) & (KA_INACTIVE_DST|KA_DISABLED_DST))

#define KA_PROBE_NONE			0
#define KA_PROBE_ALL			1
#define KA_PROBE_INACTIVE		2
#define KA_PROBE_ONLYFLAGGED	3

typedef struct _ka_dest
{
	str uri;
	int flags;
	//ds_attrs_t attrs;

	struct socket_info * sock;
	struct ip_addr ip_address; 	/*!< IP-Address of the entry */
	unsigned short int port; 	/*!< Port of the URI */
	unsigned short int proto; 	/*!< Protocol of the URI */
	//int message_count;
	struct _ka_dest *next;
} ka_dest_t;


#endif

