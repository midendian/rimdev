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
 *
 * Useful sourcess:
 *   - RFC 791         IPv4
 *   - RFC 792         ICMPv4
 *   - RFC 793         TCP
 *   - RFC 862         Echo Protocol (in TCP _and UDP!)
 *   - RFC 896         Nagle's epic
 *   - RFC 908         RDP
 *   - RFC 1016        What the hell to do with Source Quench
 *   - RFC 1025        TCP and IP Bakeoff
 *   - RFC 1045        VMTP
 *   - RFC 1072        TCP over long-fat's -- describes SACK options
 *   - RFC 1122/1123   Gateway/Host requirements RFCs
 *   - RFC 1323        Newer TCP options (RTTM, PAWS, etc)
 *   - RFC 2131        DHCP
 *
 * And of course, TCP/IP Illustrated, Vol One, WR Stevens.
 *
 */

#include <Rim.h>
#include <mobitex.h>
#include <KeyPad.h>
#include <LCD_API.h>
#include <string.h>

#include "core.h"
#include "util.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"

#define HPID_IP 242

char VersionPtr[] = APPNAME;
int AppStackSize = 16384;

/*
 * Return the checksum of the given region in the IPv4 header
 * checksum style.  Returns the value in network order.
 *
 * Note that the checksum field of the incoming header must already
 * be set to zero.
 *
 */
unsigned short csum_partial(void *buf, unsigned int len)
{
	unsigned long sum = 0;
	unsigned short *ptr = buf;

	while (len > 1)  {
		sum += *ptr++;
		len -= 2;
	}
	if (len) {
		union {
			unsigned char byte;
			unsigned short wyde;
		} odd;
		odd.wyde = 0;
		odd.byte = *((unsigned char *)ptr);
		sum += odd.wyde;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);

	return ((sum + (sum >> 16))) & 0xffff;
}

static unsigned short nextident = 0; /* XXX init this better */

#define IP_DONTFRAGMENT  0x4000
#define IP_MOREFRAGMENTS 0x2000
struct iphdr {
	unsigned char hdrlen:4 __attribute__((__packed__));
	unsigned char version:4 __attribute__((__packed__));
	unsigned char tos __attribute__((__packed__));
	unsigned short totlen __attribute__((__packed__));
	unsigned short ident __attribute__((__packed__));
	/* flags are the upper 3bits of fragoffset */
	unsigned short fragoffset __attribute__((__packed__));
	unsigned char ttl __attribute__((__packed__));
	unsigned char prot __attribute__((__packed__));
	unsigned short csum __attribute__((__packed__));
	unsigned long saddr __attribute__((__packed__));
	unsigned long daddr __attribute__((__packed__));
};

/*
 * Send an IP datagram, with the given payload and protocol number.
 *
 * destip is in network order.
 *
 */
int ip_send(unsigned long destip, unsigned char prot, unsigned char *buf, int buflen)
{
	MPAK_HEADER mpak;
	unsigned char *pkt;
	struct iphdr *hdr;
	int tag;

	if ((buflen+20) > MAX_MPAK_DATA_SIZE) {
		dprintf("ip_send: packet needs fragmentation, but its not implemented\n");
		return -1;
	}

	if (!(pkt = RimMalloc(20+buflen)))
		return -1;

	hdr = (struct iphdr *)pkt;

	hdr->version = 0x4;
	hdr->hdrlen = 5; /* we're using a standard 20 byte header */
	hdr->tos = 0x00;
	hdr->totlen = htons(20+buflen);
	hdr->ident = htons(nextident);
	//nextident += 0x4321;
	hdr->fragoffset = 0; /* XXX implement fragmentation */
	hdr->ttl = 64;
	hdr->prot = prot;
	hdr->csum = 0; /* calculated later */
	hdr->saddr = htonl(INTERMOBI_OURIP);
	hdr->daddr = destip;
	memcpy(pkt+20, buf, buflen);

	hdr->csum = ~csum_partial(pkt, 20);

	mpak.Sender = 0; /* filled in by OS */
	mpak.Destination = INTERMOBI_GATEWAYMAN;
	mpak.MpakType = MPAK_HPDATA;
	mpak.HPID = HPID_IP;
	mpak.Flags = 0;
	mpak.TrafficState = 0;
	mpak.lTime = 0;

	/*
	 * When RadioSendMpak fails with an 'out of buffers' error,
	 * InterMobi should do its own queuing.  The radio only
	 * queues seven messages total, which is not a lot for an
	 * IP stack...
	 */
	if ((tag = RadioSendMpak(&mpak, pkt, 20+buflen)) < 0)
		dprintf("ip_send: RadioSendMpak failed (%d)\n", tag);
	else
		dprintf("ip_send: %d bytes sent, tag %d\n", 20+buflen, tag);

	RimFree(pkt);

	return (tag < 0) ? -1 : 0;
}

static const struct {
	const char *name;
	unsigned char protnum;
	int (*init)(void);
	int (*handler)(unsigned long srcip, unsigned char *pkt, int pktlen);
	void (*timer)(void);
} protocols[] = {
	{"icmp", IPPROTO_ICMP, NULL, icmp_handler, NULL},
	{"tcp", IPPROTO_TCP, tcp_init, tcp_handler, tcp_timer},
	{"udp", IPPROTO_UDP, NULL, udp_handler, NULL},
	{NULL, 255, NULL, NULL}
};
 
static int ip_init(void)
{
	int i;

	for (i = 0; protocols[i].name; i++) {
		if (protocols[i].init && (protocols[i].init() != 0))
			return -1;
	}

	return 0;
}

static void ip_gottimer(void)
{
	int i;

	for (i = 0; protocols[i].name; i++) {
		if (protocols[i].timer)
			protocols[i].timer();
	}

	return;
}

static int ip_consumepacket(unsigned char *pkt, int pktlen)
{
	int i;
	struct iphdr *hdr;
	int payloadlen;

	if (!pkt || (pktlen < 20))
		return 0; /* runt! */

	hdr = (struct iphdr *)pkt;

#if 1
	dprintf("ip: version: 0x%01x\n", hdr->version);
	dprintf("ip: header length: 0x%01x (%d bytes)\n", hdr->hdrlen, hdr->hdrlen*4); 
	dprintf("ip: tos: 0x%02x\n", hdr->tos);
	dprintf("ip: total length: 0x%04x\n", ntohs(hdr->totlen));
	dprintf("ip: ident: 0x%04x\n", ntohs(hdr->ident));
	dprintf("ip: fragment offset: 0x%04x (DF = %d, MF = %d)\n", 
			 ntohs(hdr->fragoffset) & 0x1fff, 
			 (ntohs(hdr->fragoffset) & IP_DONTFRAGMENT)?1:0, 
			 (ntohs(hdr->fragoffset) & IP_MOREFRAGMENTS)?1:0);
	dprintf("ip: ttl: %d\n", hdr->ttl);
	dprintf("ip: prot: 0x%02x\n", hdr->prot);
	dprintf("ip: csum: 0x%02x\n", ntohs(hdr->csum));
	dprintf("ip: saddr: 0x%08x\n", ntohl(hdr->saddr));
	dprintf("ip: daddr: 0x%08x\n", ntohl(hdr->daddr));
#endif

	payloadlen = ntohs(hdr->totlen) - (hdr->hdrlen * 4);

	if (((ntohs(hdr->fragoffset) & 0x1f00) != 0) ||
		((ntohs(hdr->fragoffset) & IP_MOREFRAGMENTS))) {
		dprintf("ip: this packet is fragmented; dropping\n");
		return 0;
	}

	if (ntohl(hdr->saddr) == INTERMOBI_OURIP)
		return 1; /* bounced somehow */
	if (ntohl(hdr->daddr) != INTERMOBI_OURIP)
		return 1; /* not bound for us */

	for (i = 0; protocols[i].name; i++) {
		if (protocols[i].protnum == hdr->prot) {
			if (protocols[i].handler(
									 hdr->saddr,
									 pkt + (hdr->hdrlen * 4), 
									 ntohs(hdr->totlen) - (hdr->hdrlen * 4)) 
				== PKTHANDLER_ACCEPT)
				return 1;
		}
	}

	return 0;
}

static void handlecmd(char *cmd)
{
	if (strncmp(cmd, "quit", 4) == 0) {
		RimCatastrophicFailure("intermobi was told to DIE DIE DIE");
	} else if (strncmp(cmd, "dumpsk", 6) == 0) {
		tcp_dumpsockets();
	}

	return;
}

#define TIMER_INTERVAL (7*100) /* seven seconds */

#define MAXCMDLEN 128

void PagerMain(void)
{
	MESSAGE msg;
	unsigned char pktbuf[MAX_MPAK_DATA_SIZE];
	MPAK_HEADER pkthdr;
	DWORD timerid = ('I' << 24) | ('M' << 16) | ('O' << 8) | 'B';
	char cmdline[MAXCMDLEN+1] = {""};
	int i = 0;

	RimTaskYield();

	LcdGetConfig(&lcdconfig);

	dprintf("starting\n");

	if (!RimSetTimer(timerid, TIMER_INTERVAL, TIMER_PERIODIC)) {
		dprintf("RimSetTimer failed\n");
		return;
	}

	if (ip_init() != 0) {
		dprintf("ip_init failed\n");
		return;
	}

	RadioRegister();

	for (;;) {

		RimGetMessage(&msg);

		if (msg.Device == DEVICE_RADIO) {

			if (msg.Event == MESSAGE_RECEIVED) {
				int datalen;

				dprintf("radio: recieved: tag = %d, type = %d, hpid = %d\n",
						msg.SubMsg, msg.Data[0], msg.Data[1]);

				if ((msg.Data[0] == MPAK_HPDATA) && (msg.Data[1] == HPID_IP)) {

					if ((datalen = RadioGetMpak(msg.SubMsg, 
												&pkthdr, pktbuf)) == -1) {

						dprintf("        RadioGetMpak failed\n");

					} else
						ip_consumepacket(pktbuf, datalen);

				}

			} else if (msg.Event == MESSAGE_SENT) {
			} else if (msg.Event == MESSAGE_NOT_SENT) {
			} else if (msg.Event == SIGNAL_LEVEL) {
			} else if (msg.Event == NETWORK_STARTED) {
			} else if (msg.Event == BASE_STATION_CHANGE) {
			} else if (msg.Event == RADIO_TURNED_OFF) {
			} else if (msg.Event == MESSAGE_STATUS) {
			}

		} else if (msg.Device == DEVICE_KEYPAD) {

			if (msg.Event == KEY_DOWN) {

#if 0
				if (msg.SubMsg == 'c')
					tcp_dumpsockets();
#else

				if ((i < MAXCMDLEN) && 
					(((msg.SubMsg >= 'a') && (msg.SubMsg <= 'z')) ||
					 ((msg.SubMsg >= 'A') && (msg.SubMsg <= 'Z')) ||
					 (msg.SubMsg == ' ') || (msg.SubMsg == '-'))) {

					cmdline[i] = msg.SubMsg;
					i++;

					LcdPutStringXY(0, lcdconfig.height-8, cmdline, -1, 0);

				} else if (msg.SubMsg == '\r') {

					handlecmd(cmdline);

					memset(cmdline, 0, sizeof(cmdline));
					i = 0;

					LcdScroll(8);

				} else
					dprintf("unknown character: %x\n", msg.SubMsg);

#endif

			}

		} else if (msg.Device == DEVICE_TIMER) {

			if (msg.Event == timerid)
				ip_gottimer();

		}
	}

	dprintf("escaped event loop! shutting down...\n");

	RadioDeregister();

	return;
}
