/* -*- Mode: ab-c -*- */
/*
 * InterMobi.  You don't want to know.
 * 
 * Copyright (C) 2001 Adam Fritzler (ActiveBuddy, Inc)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "core.h"
#include "util.h"
#include "icmp.h"
#include <string.h> /* for memcmp */

#define ICMP_TYPE_ECHOREPLY   0x00
#define ICMP_TYPE_ECHOREQUEST 0x08

struct icmphdr {
	unsigned char type __attribute__((__packed__));
	unsigned char code __attribute__((__packed__));
	unsigned short csum __attribute__((__packed__));

	/* these two are in most messages, but not all */
	unsigned short ident __attribute__((__packed__));
	unsigned short seqnum __attribute__((__packed__));
};

/*
 * Send an ICMP Echo Reply message.
 *
 * destip, ident, and seqnum are all in network order.
 *
 */
static int icmp_send_echoreply(unsigned long destip, unsigned short ident, unsigned short seqnum, unsigned char *data, int datalen)
{
	unsigned char *buf;

	/*
	 * Allocate a buffer large enough for the ICMP message only.
	 *
	 * XXX there needs to be someway to reserve room for an IP header
	 * or something.... Lots of small allocations are Bad.
	 */
	if (!(buf = RimMalloc(4+4+datalen)))
		return -1;

	((struct icmphdr *)buf)->type = ICMP_TYPE_ECHOREPLY;
	((struct icmphdr *)buf)->code = 0;
	((struct icmphdr *)buf)->csum = 0;
	((struct icmphdr *)buf)->ident = ident;
	((struct icmphdr *)buf)->seqnum = seqnum;

	dumpbox(buf+8, datalen);
	dprintf("data = %p, datalen = %d\n", data, datalen);
	if (data && (datalen > 0))
		memcpy(buf+8, data, datalen);

	((struct icmphdr *)buf)->csum = ~csum_partial(buf, 4+4+datalen);

	if (ip_send(destip, IPPROTO_ICMP, buf, 4+4+datalen) != 0) {
		RimFree(buf);
		return -1;
	}

	RimFree(buf);

	return 0;
}

int icmp_handler(unsigned long srcip, unsigned char *pkt, int pktlen)
{
	struct icmphdr *hdr;

	if (!pkt || (pktlen < 4))
		return PKTHANDLER_REJECT;

	hdr = (struct icmphdr *)pkt;

	dprintf("icmp: type = 0x%02x\n", hdr->type);
	dprintf("icmp: code = 0x%02x\n", hdr->code);
	dprintf("icmp: csum = 0x%04x\n", ntohs(hdr->csum));


	if (hdr->type == ICMP_TYPE_ECHOREQUEST) {

		dprintf("icmp: ident = 0x%04x\n", ntohs(hdr->ident));
		dprintf("icmp: seqnum = 0x%04x\n", ntohs(hdr->seqnum));

		icmp_send_echoreply(srcip, hdr->ident, hdr->seqnum, pkt+4+4, pktlen-4-4);

	} else {

		dumpbox(pkt+4, pktlen-4);
	}

	return PKTHANDLER_ACCEPT;
}

