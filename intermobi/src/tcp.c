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
#include "tcp.h"
#include <string.h> /* for strlen */

/* This is corrected for endiannes, of course */
struct tcphdr {
	unsigned short srcport __attribute__((__packed__));
	unsigned short dstport __attribute__((__packed__));
	unsigned long seqnum __attribute__((__packed__));
	unsigned long acknum __attribute__((__packed__));
	unsigned char reserved1:4 __attribute__((__packed__));
	unsigned char hdrlen:4 __attribute__((__packed__));
	unsigned char cntrlbits:6 __attribute__((__packed__));
	unsigned char reserved2:2 __attribute__((__packed__));
	unsigned short window __attribute__((__packed__));
	unsigned short csum __attribute__((__packed__));
	unsigned short urgptr __attribute__((__packed__));
	unsigned char opts[0];
};

#define TCPBIT_FIN 0x01
#define TCPBIT_SYN 0x02
#define TCPBIT_RST 0x04
#define TCPBIT_PSH 0x08
#define TCPBIT_ACK 0x10
#define TCPBIT_URG 0x20

/* These are in arbitrary order */
#define STATE_CLOSED       0
#define STATE_LISTEN       1
#define STATE_SYN_RCVD     2
#define STATE_SYN_SENT     3
#define STATE_ESTABLISHED  4
#define STATE_CLOSE_WAIT   5
#define STATE_LAST_ACK     6
#define STATE_FIN_WAIT_1   7
#define STATE_FIN_WAIT_2   8
#define STATE_CLOSING      9
#define STATE_TIME_WAIT   10

static const char *tcp_statenames[11] = {
	"CLOSED",
	"LISTEN",
	"SYN_RCVD",
	"SYN_SENT",
	"ESTABLISHED",
	"CLOSE_WAIT",
	"LAST_ACK",
	"FIN_WAIT_1",
	"FIN_WAIT_2",
	"CLOSING",
	"TIME_WAIT",
};

#define TCP_SKID_INVAL -1
#define TCP_SKID_ORIGIN 0

#define TCP_DEFAULT_WINDOW 420 /* arbitrary */

/* This is no where near compliant. */
static unsigned long tcp_nextisn = 0x12211221;
static unsigned long tcp_genisn(void)
{

	tcp_nextisn += 0x42;

	return tcp_nextisn;
}

struct tcpvec {
	unsigned char flags; /* TCPBITS_* or zero */
	unsigned long seqnum; /* sequence number of first byte */
	int start; /* start of data in buf */
	int len; /* length of data after start */
	unsigned char *buf; /* full data buffer */
	int buflen; /* length of full data buffer... req: (start+len) <= buflen */
	int txcount; /* number of times this chunk has been sent */
	unsigned long lasttx; /* last time this chunk was transmitted (ticks) */
	struct tcpvec *next;
};

struct tcprxvec {
	int len;
	int offset;
	unsigned char *buf;
	struct tcprxvec *next;
};

/* This is packed so that it will be smaller */
struct tcp_socket {
	int id __attribute__((__packed__));
	unsigned char state __attribute__((__packed__));
	unsigned long lastchange; /* ticks of last state change */
	unsigned short localport __attribute__((__packed__));
	unsigned short remoteport __attribute__((__packed__));
	unsigned long remoteaddr __attribute__((__packed__));

	/* Next sequence number expected on incoming segments */
	unsigned long rcv_nxt __attribute__((__packed__));

	/* Oldest unacknowledged sequence number */
   	unsigned long snd_una __attribute__((__packed__));

	/* Next sequence number to be sent */
	unsigned long snd_nxt __attribute__((__packed__));

	unsigned short rcv_mss __attribute__((__packed__));

	struct tcprxvec *rxqueue;
	struct tcpvec *txqueue;

	/* 
	 * If set for a listener, it is copied to accepted sockets 
	 *
	 * Return 0 if you did not send a packet (ie, the data needs to be
	 * replied to).
	 */
	int (*datahandler)(struct tcp_socket *sk, const unsigned char *data, int datalen);
	unsigned long protflags;
};

#define TCP_MAX_OPENSOCKETS 8 /* Number of simulatenous active TCP endpoints */

/* XXX fix the .bss issue so I don't have to initalize everything! XXX */
static int tcp_nextskid = TCP_SKID_ORIGIN;
static struct tcp_socket tcp_sockets[TCP_MAX_OPENSOCKETS] = {
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, 0},
};

void tcp_dumpsockets(void)
{
	int i;

	dprintf("TCP sockets: (%d max, %d nextid)\n", 
			TCP_MAX_OPENSOCKETS, tcp_nextskid);

	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {

		if (tcp_sockets[i].id == TCP_SKID_INVAL)
			continue;

		dprintf("    (%d) %d %s local:%u <-> 0x%08x:%u\n",
				i,
				tcp_sockets[i].id,
				tcp_statenames[tcp_sockets[i].state],
				tcp_sockets[i].localport,
				tcp_sockets[i].remoteaddr,
				tcp_sockets[i].remoteport);

		dprintf("        rcv_nxt = %ld, snd_una = %ld, snd_nxt = %ld\n",
				tcp_sockets[i].rcv_nxt,
				tcp_sockets[i].snd_una,
				tcp_sockets[i].snd_nxt);

		if (tcp_sockets[i].txqueue) {
			struct tcpvec *cur;

			dprintf("      TX Queue:\n");
			for (cur = tcp_sockets[i].txqueue; cur; cur = cur->next) {
				dprintf("        %u / %ld / %d / %d / %p / %d / %lu\n",
						cur->flags, cur->seqnum, cur->start, cur->len,
						cur->buf, cur->buflen, cur->lasttx);
			}
		}

	}

	return;
}

int intermobi_tcp_socket(int skid)
{
	return -1;
}

int intermobi_tcp_listen(int skid)
{
	return -1;
}

int intermobi_tcp_write(int skid, const unsigned char *buf, int n)
{
	return -1;
}

int intermobi_tcp_read(int skid, unsigned char *buf, int n)
{
	return 0;
}

static struct tcp_socket *tcp_findsock(unsigned short localport, unsigned long remoteaddr, unsigned short remoteport)
{
	int i;

	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {

		if (tcp_sockets[i].id == TCP_SKID_INVAL)
			continue;

		if ((tcp_sockets[i].localport == localport) &&
			(tcp_sockets[i].remoteaddr == remoteaddr) &&
			(tcp_sockets[i].remoteport == remoteport))
			return tcp_sockets+i;
	}

	return NULL;
}

static struct tcp_socket *tcp_findlistener(unsigned short localport)
{
	int i;

	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {

		if (tcp_sockets[i].id == TCP_SKID_INVAL)
			continue;

		if (tcp_sockets[i].state != STATE_LISTEN)
			continue;

		if (tcp_sockets[i].localport == localport)
			return tcp_sockets+i;
	}

	return NULL;
}

static struct tcp_socket *tcp_socknew(void)
{
	int i;

	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {
		if (tcp_sockets[i].id == TCP_SKID_INVAL) {
			tcp_sockets[i].id = tcp_nextskid++;

			tcp_sockets[i].txqueue = NULL;
			tcp_sockets[i].protflags = 0;

			return tcp_sockets+i;
		}
	}

	return NULL;
}

/*
 * All state changes should pass through here so the user can be
 * notified of weird/illegal state changes.
 *
 */
static void tcp_changestate(struct tcp_socket *sk, unsigned char newstate)
{

	sk->lastchange = RimGetTicks();

	dprintf("tcp_changestate: %d [%s] -> %d [%s] (%lu)\n", 
			sk->state, tcp_statenames[sk->state],
			newstate, tcp_statenames[newstate],
			sk->lastchange);

	sk->state = newstate;

	if (sk->state == STATE_CLOSED) {

		sk->id = TCP_SKID_INVAL;

		if (sk->txqueue) {
			struct tcpvec *cur, *tmp;
			unsigned char *tmpbuf;

			for (cur = sk->txqueue; cur; ) {

				tmp = cur->next;

				/*
				 * For some reason if you directly RimFree cur->buf,
				 * the gnu linker will produce a fixup that my simulator
				 * cannot handle.  So until I feel like implementing that
				 * fixup type, workaround it. XXX
				 */
				tmpbuf = cur->buf;
				RimFree(tmpbuf);
				RimFree(cur);

				cur = tmp;
			}
		}
	}

	return;
}

static int tcp_listen(struct tcp_socket *sk, unsigned short port, int (*datahandler)(struct tcp_socket *sk, const unsigned char *data, int datalen))
{

	if (!sk)
		return -1;

	sk->localport = port;
	sk->remoteport = 0;
	sk->remoteaddr = 0;
	sk->datahandler = datahandler;
	tcp_changestate(sk, STATE_LISTEN);

	return 0;
}

/*
 * Create a new socket based on the listener with the given endpoint.
 *
 * The new socket is returned with its state set to SYN_RCVD.
 *
 */
static struct tcp_socket *tcp_accept(struct tcp_socket *listener, unsigned long remoteaddr, unsigned short remoteport, unsigned long seqnum, unsigned short mss)
{
	struct tcp_socket *sk;

	if (!(sk = tcp_socknew()))
		return NULL;

	sk->localport = listener->localport;
	sk->remoteaddr = remoteaddr;
	sk->remoteport = remoteport;
	sk->rcv_nxt = seqnum+1;
	sk->snd_una = sk->snd_nxt = tcp_genisn();
	sk->rcv_mss = mss ? mss : TCP_DEFAULT_WINDOW;
	sk->rxqueue = NULL;
	sk->txqueue = NULL;
	sk->datahandler = listener->datahandler;
	tcp_changestate(sk, STATE_SYN_RCVD);

	return sk;
}

static int tcp_send_data(struct tcp_socket *sk, const unsigned char *data, int datalen);
static int tcp_send_fin(struct tcp_socket *sk);

#define TCPPORT_ECHO    7
#define TCPPORT_DISCARD 9
#define TCPPORT_TELNET 23
#define TCPPORT_HTTP   80

static int tcp_discard_handler(struct tcp_socket *sk, const unsigned char *data, int datalen)
{

	dprintf("tcp: discard: discarding %d bytes\n", datalen);

	return 0; /* discard it! */
}

static int tcp_echo_handler(struct tcp_socket *sk, const unsigned char *data, int datalen)
{

	dprintf("tcp: echo: echoing %d bytes\n", datalen);

	tcp_send_data(sk, data, datalen); /* echo it! */

	return 1;
}

static const char loginprompt[] = {"\r\nRIM OS v2.1\r\n\r\nlogin: "};

#define TCP_TELNET_LOGGED_IN 0x00000001
static int tcp_telnet_handler(struct tcp_socket *sk, const unsigned char *data, int datalen)
{

	dprintf("tcp: telnet: got %d bytes\n", datalen);

	if (sk->protflags & TCP_TELNET_LOGGED_IN) {

		if (data[0] == '\r') {
			tcp_send_data(sk, "mid@rim:~$ \r\n", 13);
			return 1;
		}

		return 0;

	} else {
		tcp_send_data(sk, loginprompt, strlen(loginprompt));
		return 1;
	}

	return 0;
}

static const char httpindex[] = {
	" <html>"
	"    <head><title>RIM</title></head>"
	"    <body>"
	"        <hr noshade><center><h2>Someone's RIM</h2></center><hr noshade>"
	"        <font size=\"-1\"><i>InterMobi v0.01, 17 May 2001</i></font>"
	"    </body>"
	"</html>"
};

static int tcp_http_handler(struct tcp_socket *sk, const unsigned char *data, int datalen)
{
	char request[64];
	char *req = request;

	dprintf("tcp: http: got %d bytes\n", datalen);

	if (datalen <= 5) /* minimum HTTP request */
		return 0;

	if (datalen > sizeof(request)-1)
		datalen = sizeof(request)-1;

	memcpy(req, data, datalen);
	req[datalen] = '\0';

	if (strncmp(req, "GET ", 4) == 0) {

		req += 4;
		if (index(req, ' '))
			*(index(req, ' ')) = '\0';

		dprintf("tcp: http: req = %s\n", req);

		if ((strcmp(req, "/") == 0) ||
			(strncmp(req, "/index", 6) == 0)) {

			tcp_send_data(sk, httpindex, strlen(httpindex));
			tcp_send_fin(sk);
			tcp_changestate(sk, STATE_FIN_WAIT_1);
			return 1;
		}
	}

	return 0;
}

/*
 * Initialize the TCP.
 */
int tcp_init(void)
{
	int i;
	struct tcp_socket *sk;
	
	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {
		tcp_sockets[i].id = TCP_SKID_INVAL;
		tcp_sockets[i].state = STATE_CLOSED; /* do not use tcp_changestate! */
		tcp_sockets[i].txqueue = NULL;
	}

	/* discard service */
	if ((sk = tcp_socknew()))
		tcp_listen(sk, TCPPORT_DISCARD, tcp_discard_handler);

	/* echo service */
	if ((sk = tcp_socknew()))
		tcp_listen(sk, TCPPORT_ECHO, tcp_echo_handler);

	/* telnet service */
	if ((sk = tcp_socknew()))
		tcp_listen(sk, TCPPORT_TELNET, tcp_telnet_handler);

	/* http service */
	if ((sk = tcp_socknew()))
		tcp_listen(sk, TCPPORT_HTTP, tcp_http_handler);

	tcp_dumpsockets();

	return 0;
}

struct pseudohdr {
	unsigned long saddr __attribute__((__packed__));
	unsigned long daddr __attribute__((__packed__));
	unsigned char zero __attribute__((__packed__));
	unsigned char prot __attribute__((__packed__));
	unsigned short len __attribute__((__packed__));
};

static unsigned short tcp_checksum(unsigned char *buf, int buflen, unsigned long saddr, unsigned long daddr)
{
	unsigned short csum;
	struct pseudohdr phdr;

	phdr.saddr = saddr;
	phdr.daddr = daddr;
	phdr.zero = 0;
	phdr.prot = IPPROTO_TCP;
	phdr.len = htons(buflen);

	csum = ~((ntohs(csum_partial(&phdr, 12)) + 
			 ntohs(csum_partial(buf, buflen))) & 0xffff);

	csum = htons(csum);

	if (csum == 0x0000)
		csum = 0xffff;

	return csum;
}

/*
 * Send a RST to destip, from port srcport to port dstport, acking seqnum.
 */
static int tcp_send_rst(unsigned long destip, unsigned short dstport, unsigned short srcport, unsigned long seqnum)
{
	unsigned char pkt[20];
	struct tcphdr *hdr = (struct tcphdr *)pkt;

	hdr->srcport = srcport;
	hdr->dstport = dstport;
	hdr->seqnum = 0; /* not used in this, right? */
	hdr->acknum = htonl(ntohl(seqnum)+1);
	hdr->hdrlen = 5; /* 20 / 4 = 5 */
	hdr->reserved1 = hdr->reserved2 = 0;
	hdr->cntrlbits = TCPBIT_RST | TCPBIT_ACK;
	hdr->window = 0;
	hdr->csum = 0;
	hdr->urgptr = 0;

	hdr->csum = tcp_checksum(pkt, hdr->hdrlen*4, htonl(INTERMOBI_OURIP), destip);

	ip_send(destip, IPPROTO_TCP, pkt, 20);

	return 0;
}

static void tcp_txenqueue(struct tcp_socket *sk, unsigned char flags, unsigned long seqnum, unsigned char *buf, int buflen)
{
	struct tcpvec *vec, *cur;

	if (!(vec = RimMalloc(sizeof(struct tcpvec))))
		return;

	vec->flags = flags;
	vec->seqnum = sk->snd_nxt;
	vec->start = 0;
	vec->len = buflen;
	vec->buf = buf;
	vec->buflen = buflen;
	vec->next = NULL;
	vec->lasttx = 0;
	vec->txcount = 0;

	if (!sk->txqueue)
		sk->txqueue = vec;
	else {
		for (cur = sk->txqueue; cur->next; cur = cur->next)
			;
		cur->next = vec;
	}

	if (flags & TCPBIT_SYN)
		sk->snd_nxt++;
	sk->snd_nxt += buflen;
	if (flags & TCPBIT_FIN)
		sk->snd_nxt++;

	return;
}

/*
 * Send a SYN-ACK to destip, from port srcport to port dstport, acking seqnum.
 *
 * XXX this only does passive opens, not active opens.
 *
 * XXX make this use the queue and tcp_txenqueue().
 */
static int tcp_send_syn(struct tcp_socket *sk)
{

	tcp_txenqueue(sk, TCPBIT_SYN, sk->snd_nxt /*ntohl(hdr->seqnum)*/, NULL, 0);

	tcp_send_data(sk, NULL, 0);

	return 0;
}

static int tcp_send_fin(struct tcp_socket *sk)
{

	tcp_txenqueue(sk, TCPBIT_FIN, sk->snd_nxt, NULL, 0);

	tcp_send_data(sk, NULL, 0);

	return 0;
}

static unsigned char *bufdup(const unsigned char *buf, int buflen)
{
	unsigned char *ret;

	if (!(ret = RimMalloc(buflen)))
		return NULL;

	memcpy(ret, buf, buflen);

	return ret;
}

/*
 * This is one of the main 'oh dear gods what a hack this is' points,
 * and a point of major incompliance with the RFC.
 *
 * XXX Might want to do this Right someday, which involves the
 * smoothed RTT estimator and all that.
 *
 */
#define TCP_MAX_RETRIES 5

/*
 * This is a bit complicated because we attempt to stuff everything
 * we've already sent that hasn't been acked into this packet, along
 * with the new data.
 *
 * XXX check the endpoint window and MSS to make sure we don't overflow it.
 *
 */
static int tcp_send_data(struct tcp_socket *sk, const unsigned char *data, int datalen)
{
	unsigned char *pkt;
	struct tcphdr *hdr;
	struct tcpvec *cur;
	unsigned short flags = 0;
	unsigned long seqnum;

	/* First, enqueue the new data and increment the next seqnum */
	if (data && datalen)
		tcp_txenqueue(sk, 0, sk->snd_nxt, bufdup(data, datalen), datalen);

	for (cur = sk->txqueue, datalen = 0, seqnum = sk->snd_nxt; 
		 cur && (datalen < sk->rcv_max); cur = cur->next) {

		if (cur->txcount >= TCP_MAX_RETRIES) {
			tcp_changestate(sk, STATE_TIME_WAIT);
			tcp_send_fin(sk);
			return -1;
		}

		flags |= cur->flags;
		if (cur->seqnum < seqnum)
			seqnum = cur->seqnum; /* get the lowest seqnum queued */
		datalen += cur->len;
	}

	dprintf("tcp_send_data: procesing %d queued bytes\n", datalen);

	if (!(pkt = RimMalloc(20+datalen)))
		return -1;

	hdr = (struct tcphdr *)pkt;

	hdr->srcport = htons(sk->localport);
	hdr->dstport = htons(sk->remoteport);

	if (sk->snd_nxt < sk->snd_una)
		sk->snd_una = sk->snd_nxt;

	hdr->seqnum = htonl(seqnum);
	hdr->acknum = htonl(sk->rcv_nxt);

	hdr->hdrlen = 5; /* 20 / 4 = 5 */
	hdr->reserved1 = hdr->reserved2 = 0;
	hdr->cntrlbits = TCPBIT_ACK | flags | (!flags ? TCPBIT_PSH : 0);
	hdr->window = htons(TCP_DEFAULT_WINDOW);
	hdr->csum = 0;
	hdr->urgptr = 0;

	for (cur = sk->txqueue, flags = 20; cur; cur = cur->next) {
		memcpy(pkt+flags, cur->buf+cur->start, cur->len);
		flags += cur->len;
		cur->lasttx = RimGetTicks();
		cur->txcount++;
	}

	hdr->csum = tcp_checksum(pkt, (hdr->hdrlen*4)+datalen, 
							 htonl(INTERMOBI_OURIP), htonl(sk->remoteaddr));

	ip_send(htonl(sk->remoteaddr), IPPROTO_TCP, pkt, 20+datalen);

	RimFree(pkt);

	return 0;
}

static int tcp_send_ack(struct tcp_socket *sk)
{
	unsigned char pkt[20];
	struct tcphdr *hdr;

	hdr = (struct tcphdr *)pkt;

	hdr->srcport = htons(sk->localport);
	hdr->dstport = htons(sk->remoteport);

	hdr->seqnum = htonl(sk->snd_nxt); /* do not consume a seqnum! */
	hdr->acknum = htonl(sk->rcv_nxt);

	hdr->hdrlen = 5; /* 20 / 4 = 5 */
	hdr->reserved1 = hdr->reserved2 = 0;
	hdr->cntrlbits = TCPBIT_ACK;
	hdr->window = htons(TCP_DEFAULT_WINDOW);
	hdr->csum = 0;
	hdr->urgptr = 0;

	hdr->csum = tcp_checksum(pkt, hdr->hdrlen*4, htonl(INTERMOBI_OURIP), htonl(sk->remoteaddr));

	ip_send(htonl(sk->remoteaddr), IPPROTO_TCP, pkt, 20);

	return 0;
}

static int tcp_consumeseg(struct tcp_socket *sk, unsigned long srcip, unsigned char *pkt, int pktlen)
{
	struct tcphdr *hdr = (struct tcphdr *)pkt;

	sk->rcv_nxt = ntohl(hdr->seqnum)+1;

	if (hdr->cntrlbits & TCPBIT_ACK) {
		struct tcpvec *cur, **prev;
		unsigned long newoldest, curack;

		newoldest = sk->snd_nxt;
		curack = ntohl(hdr->acknum);

		if (curack < sk->snd_una)
			return 0; /* been there, done that */

		tcp_dumpsockets();
		for (prev = &sk->txqueue; (cur = *prev); ) {

			if (cur->seqnum < curack) {
				int offset;

				offset = curack - cur->seqnum;
				dprintf("tcp_consumeseq: %d-%d have been acked\n", 
						cur->seqnum, cur->seqnum+offset);

				if (offset >= cur->len) {

					*prev = cur->next;

					if (cur->flags & TCPBIT_SYN)
						tcp_changestate(sk, STATE_ESTABLISHED);

					RimFree(cur->buf);
					RimFree(cur);

				} else {

					cur->seqnum += offset;
					cur->start += offset;
					cur->len -= offset;
					dprintf("tcp_consumeseq: still %d left in this vec\n");

					prev = &cur->next;
				}

			} else {

				if (cur->seqnum < newoldest)
					newoldest = cur->seqnum;

				prev = &cur->next;
			}
		}

		sk->snd_una = newoldest;
	}

	if ((pktlen - (hdr->hdrlen*4)) > 0) { /* we have data! */

		dprintf("*********REAL DATA!!! %u+(%u-%u)=%u\n", ntohl(hdr->seqnum), pktlen, hdr->hdrlen*4, ntohl(hdr->seqnum)+(pktlen-(hdr->hdrlen*4)));
		dumpbox(pkt+(hdr->hdrlen*4), pktlen-(hdr->hdrlen*4));

		/* XXX implement delayed ACKs and mitigate ACK-only segments */
		sk->rcv_nxt = ntohl(hdr->seqnum)+(pktlen-(hdr->hdrlen*4))+0;
		if (!sk->datahandler || 
			!sk->datahandler(sk, pkt+(hdr->hdrlen*4), pktlen-(hdr->hdrlen*4)))
			tcp_send_ack(sk);
	}

	if (hdr->cntrlbits & TCPBIT_FIN) {

		dprintf("connection closed!\n");

		/* No need to send a seperate ACK, it will be in the FIN */
		//sk->rcv_nxt = ntohl(hdr->seqnum)+1;

		if (sk->state == STATE_FIN_WAIT_1) {
			tcp_changestate(sk, STATE_TIME_WAIT);
		} else if (sk->state != STATE_TIME_WAIT) { 
			tcp_send_fin(sk);
			tcp_changestate(sk, STATE_FIN_WAIT_1);
		}

	}

	return 0;
}

/*
 * Handle an incoming TCP segment.
 */
int tcp_handler(unsigned long srcip, unsigned char *pkt, int pktlen)
{
	struct tcphdr *hdr;
	int optlen, i;
	struct tcp_socket *sk;
	unsigned short mss = 0;

	if (!pkt || (pktlen < 20))
		return PKTHANDLER_REJECT;

	hdr = (struct tcphdr *)pkt;

	dprintf("tcp: srcport = %d / dstport = %d\n",
			ntohs(hdr->srcport), ntohs(hdr->dstport));
	dprintf("tcp: seqnum = 0x%08x / acknum = 0x%08x\n",
			ntohl(hdr->seqnum), ntohl(hdr->acknum));
	dprintf("tcp: hdrlen = 0x%01x (%d bytes)\n",
			hdr->hdrlen, hdr->hdrlen*4);
	dprintf("tcp: reserved1 = 0x%01x / reserved2 = 0x%01x\n",
			hdr->reserved1, hdr->reserved2);
	dprintf("tcp: control bits: 0x%02x = %s%s%s%s%s%s\n",
			hdr->cntrlbits,
			(hdr->cntrlbits & TCPBIT_FIN) ? "FIN " : "",
			(hdr->cntrlbits & TCPBIT_SYN) ? "SYN " : "",
			(hdr->cntrlbits & TCPBIT_RST) ? "RST " : "",
			(hdr->cntrlbits & TCPBIT_PSH) ? "PSH " : "",
			(hdr->cntrlbits & TCPBIT_ACK) ? "ACK " : "",
			(hdr->cntrlbits & TCPBIT_URG) ? "URG " : "");
	dprintf("tcp: window = %d\n", ntohs(hdr->window));
	dprintf("tcp: csum = 0x%04x\n", ntohs(hdr->csum));
	dprintf("tcp: urgptr = 0x%04x\n", ntohs(hdr->urgptr));

	/* Calculate the length of the options section */
	optlen = (hdr->hdrlen * 4) - 20; /* base header is always 20 bytes */

	for (i = 0; i < optlen; ) {

		if (hdr->opts[i] == 0x00) /* EOO */
			break;

		if (hdr->opts[i] == 0x01) { /* NOP */
			dprintf("tcp: opts: nop\n");
			i++;
			continue;
		}

		if (hdr->opts[i] == 0x02) { /* MSS */
			unsigned short *mss = (unsigned short *)(hdr->opts+i+2);

			dprintf("tcp: opts: mss = %d\n", ntohs(*mss));

			mss = ntohs(*mss);

		} else if (hdr->opts[i] == 0x03) {
			unsigned char *shift = hdr->opts+i+2;

			dprintf("tcp: opts: wscale = %d\n", *shift);

		} else if (hdr->opts[i] == 0x04) {

			dprintf("tcp: opts: sack permitted\n");

		} else if (hdr->opts[i] == 0x08) {
			unsigned long *stamp = (unsigned long *)(hdr->opts+i+2);
			unsigned long *reply = (unsigned long *)(hdr->opts+i+2+4);

			dprintf("tcp: opts: timestamp = %d %d\n", ntohl(*stamp), ntohl(*reply));

		} else
			dprintf("tcp: opts: unknown: kind = %d / len = %d\n", hdr->opts[i], hdr->opts[i+1]);

		i += hdr->opts[i+1];
	}

	if (!(sk = tcp_findsock(ntohs(hdr->dstport), 
							ntohl(srcip), ntohs(hdr->srcport)))) {

		if (hdr->cntrlbits & TCPBIT_SYN) {
			struct tcp_socket *lsk;

			if ((lsk = tcp_findlistener(ntohs(hdr->dstport))) &&
				(sk = tcp_accept(lsk, ntohl(srcip), ntohs(hdr->srcport), 
								 ntohl(hdr->seqnum), mss))) {

				dprintf("tcp: connecting 0x%08lx:%u to local port %u\n",
						srcip, hdr->srcport,
						lsk->localport);
				
				tcp_send_syn(sk);

			} else {
				dprintf("tcp: unable to find matching socket for incoming connection; sending RST\n");
				tcp_send_rst(srcip, hdr->srcport, hdr->dstport, hdr->seqnum);
			}

		} else {
			dprintf("tcp: unable to find matching socket\n");

#if 0
			/* 
			 * I don't think this is compliant behavior. Linux doesn't
			 * like it, anyway.
			 *
			 * Which leads to the question... How the hell do you get out
			 * FIN_WAIT1/2, other than letting it time out?
			 *
			 */
			tcp_send_rst(srcip, hdr->srcport, hdr->dstport, hdr->seqnum);
#endif
		}

	} else {

		tcp_consumeseg(sk, srcip, pkt, pktlen);

	}

	return PKTHANDLER_ACCEPT;
}

#define TIME_WAIT_DELAY (30*100) /* 30 seconds */

void tcp_timer(void)
{
	int i;

	dprintf("tcp_timer!\n");

	for (i = 0; i < TCP_MAX_OPENSOCKETS; i++) {

		if (tcp_sockets[i].id == TCP_SKID_INVAL)
			continue;

		if (tcp_sockets[i].state == STATE_CLOSED)
			continue;

		if (tcp_sockets[i].state == STATE_TIME_WAIT) {
			if ((RimGetTicks() - tcp_sockets[i].lastchange) > TIME_WAIT_DELAY)
				tcp_changestate(tcp_sockets+i, STATE_CLOSED);
		}

		if (tcp_sockets[i].txqueue)
			tcp_send_data(tcp_sockets+i, NULL, 0);
	}



	return;
}
