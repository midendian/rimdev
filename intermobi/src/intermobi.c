/* -*- Mode: ab-c -*- */

/*
 * InterMobi.  You don't want to know.
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
#include <string.h>

#define HPID_IP 242

#define APPNAME "intermobi"
#define DPRINTF_PREFIX APPNAME ": "

char VersionPtr[] = APPNAME;
int AppStackSize = 16384;


#define dprintf(x, y...) RimDebugPrintf(DPRINTF_PREFIX x, ##y)
#define dinlineprintf(x, y...) RimDebugPrintf(x, ##y)

static void dumpbox(const unsigned char *buf, int buflen)
{
	int i;

	for (i = 0; i < buflen; i++) {

		if (!(i % 8)) {
			if (i)
				dinlineprintf("\n");
			dprintf("        ");
		}

		dinlineprintf("%02x ", buf[i]);
	}

	dinlineprintf("\n");

	return;
}

#define ntohs(x) ( \
    (((x) & 0x00ff) << 8) | \
    (((x) & 0xff00) >> 8)   \
)

#define ntohl(x) ( \
    (((x) & 0x000000ff) << 24) | \
    (((x) & 0x0000ff00) <<  8) | \
    (((x) & 0x00ff0000) >>  8) | \
    (((x) & 0xff000000) >> 24)   \
)

#define htons(x) ntohs(x)
#define htonl(x) ntohl(x)

/*
 * Return the checksum of the given region in the IPv4 header
 * checksum style.  Returns the value in network order.
 *
 * Note that the checksum field of the incoming header must already
 * be set to zero.
 *
 */
static unsigned short csum_partial(void *buf, unsigned int len)
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

#define PKTHANDLER_REJECT 0
#define PKTHANDLER_ACCEPT 1

#if 0
#define INTERMOBI_OURIP      0x0a000402
#else
#define INTERMOBI_OURIP      0xc0a80afa
#endif
#define INTERMOBI_GATEWAYMAN 102009

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

#define IPPROTO_ICMP 0x01
#define IPPROTO_TCP  0x06
#define IPPROTO_UDP  0x11

/*
 * Send an IP datagram, with the given payload and protocol number.
 *
 * destip is in network order.
 *
 */
static int ip_send(unsigned long destip, unsigned char prot, unsigned char *buf, int buflen)
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

static int icmp_handler(unsigned long srcip, unsigned char *pkt, int pktlen)
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
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
	{TCP_SKID_INVAL, STATE_CLOSED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0},
};

static void tcp_dumpsockets(void)
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
static struct tcp_socket *tcp_accept(struct tcp_socket *listener, unsigned long remoteaddr, unsigned short remoteport, unsigned long seqnum)
{
	struct tcp_socket *sk;

	if (!(sk = tcp_socknew()))
		return NULL;

	sk->localport = listener->localport;
	sk->remoteaddr = remoteaddr;
	sk->remoteport = remoteport;
	sk->rcv_nxt = seqnum+1;
	sk->snd_una = 0; /* set by tcp_send_syn() */
	sk->snd_nxt = tcp_genisn();
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

static char *index(const char *s, int c)
{

	while (s && *s) {
		if (*s == (char)c)
			return (char *)s;
		s++;
	}

	return NULL;
}

static int strncmp(const char *s1, const char *s2, int n)
{
	int i = 0;

	while (s1 && s2 && *s1 && *s2 && (i < n)) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
		i++;
	}

	return 0;
}

static int strcmp(const char *s1, const char *s2)
{

	while (s1 && s2 && *s1 && *s2) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
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
static int tcp_init(void)
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

	csum = ~(ntohs(csum_partial(&phdr, 12)) + 
			 ntohs(csum_partial(buf, buflen))) & 0xffff;

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
	unsigned char *pkt;
	struct tcphdr *hdr;

	if (!(pkt = RimMalloc(20)))
		return -1;

	hdr = (struct tcphdr *)pkt;

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

	RimFree(pkt);

	return 0;
}

static void tcp_txenqueue(struct tcp_socket *sk, unsigned char flags, unsigned long seqnum, unsigned char *buf, int buflen)
{
	struct tcpvec *vec, *cur;

	if (!(vec = RimMalloc(sizeof(struct tcpvec))))
		return;

	vec->flags = flags;
	vec->seqnum = seqnum;
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

	return;
}

#define TCP_DEFAULT_WINDOW 2144 /* arbitrary */

/*
 * Send a SYN-ACK to destip, from port srcport to port dstport, acking seqnum.
 *
 * XXX this only does passive opens, not active opens.
 *
 * XXX make this use the queue and tcp_txenqueue().
 */
static int tcp_send_syn(struct tcp_socket *sk)
{
#if 0
	unsigned char *pkt;
	struct tcphdr *hdr;

	if (!(pkt = RimMalloc(24)))
		return -1;

	hdr = (struct tcphdr *)pkt;

	hdr->srcport = htons(sk->localport);
	hdr->dstport = htons(sk->remoteport);

	/* SYNs consume a seqnum */
	sk->snd_una = sk->snd_nxt;
	hdr->seqnum = htonl(sk->snd_nxt);
	sk->snd_nxt++;

	hdr->acknum = htonl(sk->rcv_nxt);

	hdr->hdrlen = 6; /* 24 / 4 = 6 */
	hdr->reserved1 = hdr->reserved2 = 0;
	hdr->cntrlbits = TCPBIT_SYN | TCPBIT_ACK;
	hdr->window = htons(TCP_DEFAULT_WINDOW);
	hdr->csum = 0;
	hdr->urgptr = 0;

	pkt[20] = 0x02;
	pkt[21] = 0x04;
	*(unsigned short *)(pkt+22) = htons(536); /* arbitrary? */

	hdr->csum = tcp_checksum(pkt, hdr->hdrlen*4, htonl(INTERMOBI_OURIP), htonl(sk->remoteaddr));

	ip_send(htonl(sk->remoteaddr), IPPROTO_TCP, pkt, 24);
#endif

	tcp_txenqueue(sk, TCPBIT_SYN, sk->snd_nxt /*ntohl(hdr->seqnum)*/, NULL, 0);

	tcp_send_data(sk, NULL, 0);

#if 0
	RimFree(pkt);
#endif

	return 0;
}

/* XXX make this use txenqueue() */
static int tcp_send_fin(struct tcp_socket *sk)
{
#if 0
	unsigned char *pkt;
	struct tcphdr *hdr;

	if (!(pkt = RimMalloc(20)))
		return -1;

	hdr = (struct tcphdr *)pkt;

	hdr->srcport = htons(sk->localport);
	hdr->dstport = htons(sk->remoteport);

	/* FINs consume a seqnum */
	sk->snd_una = sk->snd_nxt;
	hdr->seqnum = htonl(sk->snd_nxt);
	sk->snd_nxt++;

	hdr->acknum = htonl(sk->rcv_nxt);

	hdr->hdrlen = 5; /* 20 / 4 = 5 */
	hdr->reserved1 = hdr->reserved2 = 0;
	hdr->cntrlbits = TCPBIT_FIN | TCPBIT_ACK;
	hdr->window = htons(TCP_DEFAULT_WINDOW);
	hdr->csum = 0;
	hdr->urgptr = 0;

	hdr->csum = tcp_checksum(pkt, hdr->hdrlen*4, htonl(INTERMOBI_OURIP), htonl(sk->remoteaddr));

	ip_send(htonl(sk->remoteaddr), IPPROTO_TCP, pkt, 20);
#endif

	tcp_txenqueue(sk, TCPBIT_FIN, sk->snd_nxt, NULL, 0);

#if 0
	RimFree(pkt);
#endif

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
	if (data && datalen) {
		tcp_txenqueue(sk, 0, sk->snd_nxt, bufdup(data, datalen), datalen);
		sk->snd_nxt += datalen;
	}

	for (cur = sk->txqueue, datalen = 0, seqnum = sk->snd_nxt; 
		 cur; cur = cur->next) {

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
	unsigned char *pkt;
	struct tcphdr *hdr;

	if (!(pkt = RimMalloc(20)))
		return -1;

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

	RimFree(pkt);

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

		dprintf("REAL DATA!!!\n");
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
static int tcp_handler(unsigned long srcip, unsigned char *pkt, int pktlen)
{
	struct tcphdr *hdr;
	int optlen, i;
	struct tcp_socket *sk;

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
								 ntohl(hdr->seqnum)))) {

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

static void tcp_timer(void)
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


static int udp_handler(unsigned long srcip, unsigned char *pkt, int pktlen)
{
	return PKTHANDLER_REJECT;
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

#define TIMER_INTERVAL (7*100) /* seven seconds */

void PagerMain(void)
{
	MESSAGE msg;
	unsigned char pktbuf[MAX_MPAK_DATA_SIZE];
	MPAK_HEADER pkthdr;
	DWORD timerid = ('I' << 24) | ('M' << 16) | ('O' << 8) | 'B';

	RimTaskYield();

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

				if (msg.SubMsg == 'c')
					tcp_dumpsockets();

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
