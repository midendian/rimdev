/* -*- Mode: ab-c -*- */

/*
 * Define RIM_USETAP to enable sending/recieving datagrams of a certain
 * HPID on a tun/tap (formerly ethertap) network interface.  This can be
 * used in combination with a real radio, as long as the HPID doesn't
 * conflict with HPID's you want to use on the real radio.
 *
 * On initialization, the tun0 device will be dynamically created (via
 * /dev/net/tun and an ioctl). Before any data can be passed and the pipe
 * to be functional, someone must give it an IP address, shut ARP off, 
 * give an endpoint address, and set the MTU to something resonable for 
 * MOBITEX:
 *
 * /sbin/ifconfig tun0 \
 *    10.0.4.1 netmask 255.255.255.0 broadcast 10.0.4.255 \
 *    pointopoint 10.0.4.2 -arp mtu 512
 * 
 * This will set up your RIM subnet to be 10.0.4.0/24.  You can then ping
 * the emulated RIM, at 10.0.4.2, assuming you're running InterMobi:
 *   [root@hanwavel /root]# ping 10.0.4.2
 *   PING 10.0.4.2 (10.0.4.2) from 10.0.4.1 : 56(84) bytes of data.
 *   64 bytes from 10.0.4.2: icmp_seq=0 ttl=64 time=495.951 msec
 *   64 bytes from 10.0.4.2: icmp_seq=1 ttl=64 time=400.697 msec
 *   64 bytes from 10.0.4.2: icmp_seq=2 ttl=64 time=365.446 msec
 *   64 bytes from 10.0.4.2: icmp_seq=3 ttl=64 time=400.564 msec
 *   64 bytes from 10.0.4.2: icmp_seq=4 ttl=64 time=470.560 msec
 *
 *   --- 10.0.4.2 ping statistics ---
 *   5 packets transmitted, 5 packets received, 0% packet loss
 *   round-trip min/avg/max/mdev = 365.446/426.643/495.951/48.648 ms
 *
 *
 * Note that because of the way TUN/TAP works, ifconfig must be rerun every
 * time the simulator is invoked, as the device is dynamically created and
 * destroyed based on whether there is an open file descriptor attached to
 * it or not.  This is annoying.  It would probably be possible to have
 * the simulator set the IP address... but then, you'd have to run the
 * simulator set-uid root.  (You don't have to do this now because all
 * you need to do is give yourself adequate permission to mutilate 
 * /dev/net/tun.)
 *
 */
#define RIM_USETAP

/*
 * HPID to use for IP datagrams.
 */
#define RIM_TAP_HPID 242

/* Forms an IP address in host byte order */
#define MAKEIP(a,b,c,d) ((((a)&0xff) << 24) | \
						 (((b)&0xff) << 16) | \
						 (((c)&0xff) << 8) |  \
						 ((d)&0xff))
/* 
 * Our IP address (10.0.4.2) 
 *
 * This is the IP address of the simulated RIM handheld.  Packets
 * destined to this IP address will be packaged into a MOBITEX HPDATA
 * packet with an HPIP of RIM_TAP_HPID and set running applications.
 * MPAKs sent by applications with RIM_TAP_HPID set will have this
 * address marked as their source.
 *
 * Theoretically, two simulators running on the same machine with
 * two different values for this parameter will be able to talk to
 * each other.
 *
 */
#define RIM_TAP_OURIP MAKEIP(10,0,4,2)

/* 
 * The Gateway IP address (10.0.4.1) 
 *
 * This should be set to the address of tun0.
 *
 */
#define RIM_TAP_GATEWAYIP MAKEIP(10,0,4,1)

/*
 * The Gateway MAN number. 
 *
 * This should be set to whatever MAN number is acting as the
 * IP gateway for this pager.  You can probably just make something
 * up, unless you're using a real radio too.
 *
 */
#define RIM_TAP_GATEWAYMAN 102009

#include <rimsim.h>
#include "radio.h"
#include "messages.h" /* for enqueuemsg */

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <linux/netdevice.h>
#include <linux/if_tun.h>

static struct {
	int state; /* 0 = off, 1 = on */
	int rssi; 
	unsigned long man;
	unsigned long esn;
} radio_info = {
	0, /* off */
	RSSI_NO_COVERAGE, 
	15800586, /* MAN */
	33441111, /* ESN */
};
static pthread_mutex_t radio_lock = PTHREAD_MUTEX_INITIALIZER;

struct mpak_slot {
	int occupied;
	int slotnum;
	TASK sender;
	int done; /* nonzero if transmittion is complete */
	int direction; /* 0 = App->Cloud, 1 = Could->App */
	int refcount; /* will be reclaimed if zero */
	unsigned char *buf;
	int buflen;
};

static struct mpak_slot mpak_slots[MAX_QUEUED_MPAKS];
static pthread_mutex_t mpak_slot_lock = PTHREAD_MUTEX_INITIALIZER;

struct radio_thread_args {
	int rapfd;
	int tapfd;
	char tapname[16];
};

static int onetransmit(struct radio_thread_args *args);

static void dumpbox(const unsigned char *buf, int len)
{
  int i;

  if (!buf || (len <= 0))
    return;

  for (i = 0; i < len; i++) {
    if ((i % 8) == 0)
      printf("\n\t\t\t");

    printf("0x%2x ", buf[i]);
  }
  printf("\n\n");

  return;
}

static void forwardip(const unsigned char *inbuf, int inbuflen)
{
 	unsigned char *buf;
	int buflen;
	int i = 0;
	unsigned long destman;
	unsigned long srcman = RIM_TAP_GATEWAYMAN;

	dumpbox(inbuf, inbuflen);

	destman = radio_getman();

	buflen = 11 + 1 + inbuflen;

	if (!(buf = malloc(buflen)))
		return;

	buf[i++] = (srcman >> 16) & 0xff;
	buf[i++] = (srcman >>  8) & 0xff;
	buf[i++] = (srcman      ) & 0xff;
	buf[i++] = (destman>> 16) & 0xff;
	buf[i++] = (destman>>  8) & 0xff;
	buf[i++] = (destman     ) & 0xff;
	buf[i++] = 0x00;
	buf[i++] = 4; /* MPAK_TYPE_HPDATA */
	buf[i++] = 0x00;
	buf[i++] = 0x00;
	buf[i++] = 0x00;
	buf[i++] = RIM_TAP_HPID;

	memcpy(buf+i, inbuf, inbuflen);

	radio_recvmpak(4 /* MPAK_TYPE_HPDATA */, 
				   RIM_TAP_HPID, 
				   buf, buflen);

	free(buf);

	return;
}

static void *radio_thread(void *arg)
{
	struct radio_thread_args *args = (struct radio_thread_args *)arg;
	fd_set fds;
	int maxfd;
	struct timeval tv;
	int selret;

	if ((args->rapfd == -1) && (args->tapfd == -1)) {
		free(args);
		return NULL; /* we can't do anything */
	}

	radio_setstate(1); /* turn radio on */

	if (args->tapfd != -1) {
		pthread_mutex_lock(&radio_lock);
		radio_info.rssi = -80; /* pretty good */
		pthread_mutex_unlock(&radio_lock);
	}

	maxfd = (args->tapfd > args->rapfd) ? args->tapfd : args->rapfd;

	for (;;) {

		FD_ZERO(&fds);
		if (args->tapfd != -1)
			FD_SET(args->tapfd, &fds);
		if (args->rapfd != -1)
			FD_SET(args->rapfd, &fds);

		/* timeout needed for transmit polling */
		tv.tv_sec = 0;
		tv.tv_usec = 50000; /* XXX get rid of this shit */

		if ((selret = select(maxfd+1, &fds, NULL, NULL, &tv)) == -1) {
			perror("radio_thread: select");
			break;
		}

		if (onetransmit(args) == -1)
			break;

		if (selret == 0)
			continue;

		if ((args->rapfd != -1) && FD_ISSET(args->rapfd, &fds))
			;

		if ((args->tapfd != -1) && FD_ISSET(args->tapfd, &fds)) {
			int len;
			union { /* this _must_ be packed properly! */
				struct {
					/* The pi field from TUN/TAP is misdocumented as 32bits */
					//unsigned long pi  __attribute__((__packed__));
					//char etherhdr[14] __attribute__((__packed__));
					struct iphdr ip __attribute__((__packed__));
				} fmt;
				unsigned char raw[65536] __attribute__((__packed__));
			} u;

			if ((len = read(args->tapfd, &u, sizeof(u))) <= 0) {
				perror("radio_thread: tap: read");
				break;
			}

			fprintf(stderr, "radio_thread: tap: read %d bytes from %s\n", len, args->tapname);
#if 1
			dumpbox(u.raw, len);
#endif

#if 1
			fprintf(stderr, "tap: SRC = %u.%u.%u.%u DST = %u.%u.%u.%u PROT = 0x%02x\n",
					(ntohl(u.fmt.ip.saddr) >> 24) & 0xFF,
					(ntohl(u.fmt.ip.saddr) >> 16) & 0xFF,
					(ntohl(u.fmt.ip.saddr) >> 8) & 0xFF,
					(ntohl(u.fmt.ip.saddr) >> 0) & 0xFF,
					(ntohl(u.fmt.ip.daddr) >> 24) & 0xFF,
					(ntohl(u.fmt.ip.daddr) >> 16) & 0xFF,
					(ntohl(u.fmt.ip.daddr) >> 8) & 0xFF,
					(ntohl(u.fmt.ip.daddr) >> 0) & 0xFF,
					u.fmt.ip.protocol);
#endif

			if (ntohs(u.fmt.ip.tot_len) > 512) {
				fprintf(stderr, "tap: packet too large! (%d)  please set the MTU on tun0 to be 512\n", ntohs(u.fmt.ip.tot_len));
				continue;
			}

			if (ntohl(u.fmt.ip.daddr) != RIM_TAP_OURIP) {
				fprintf(stderr, "radio: this packet not destined for us\n");
				continue;
			}

#if 0
			switch (u.fmt.ip.protocol) {
			case IPPROTO_ICMP: {
				struct icmphdr *icmp = (void *)((u_int32_t *)&u.fmt.ip + u.fmt.ip.ihl );

				if (icmp->type == ICMP_ECHO) {
					fprintf(stderr, "tap: PING! (iphdr = %u bytes)\n",
							(unsigned int)((char *)icmp - (char *)&u.fmt.ip));
				}
				break;
			}
			}	
#endif

			forwardip(u.raw, len);

		}

	}

	radio_setstate(0); /* shut the radio off */

	free(args);

	return NULL;
}

#ifdef RIM_USETAP
/*
 * This code stolen from Documentation/network/tuntap.txt in a
 * linux 2.4 tree. It was modified in minor ways to fit my style.
 *
 * One would think that using the TUN interface would make more
 * sense than using the TAP interface.  However, because eventually
 * we want to be able to have more than one RIM on the same subnet,
 * we need TAP because it emulates a shared media, where TUN
 * emulates a point-to-point media (meaning we could only have
 * one RIM per tun).
 */
static int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	if ((fd = open("/dev/net/tun", O_RDWR)) == -1)
		return -1;

	memset(&ifr, 0, sizeof(ifr));

	/* 
	 * Flags: IFF_TUN   - TUN device (no Ethernet headers)
	 *        IFF_TAP   - TAP device
	 *
	 *        IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	if(dev && strlen(dev))
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) == -1) {
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);

	return fd;
}
#endif /* RIM_USETAP */

int radio_init(const char *radiofn)
{
	pthread_t radiotid;
	int i;
	struct radio_thread_args *args;

	fprintf(stderr, "radio: ourip = 0x%08x, gateway = 0x%08x\n", RIM_TAP_OURIP, RIM_TAP_GATEWAYIP);

	if (!(args = malloc(sizeof(struct radio_thread_args))))
		return -1;
	args->rapfd = -1;
	args->tapfd = -1;
	strcpy(args->tapname, "tun0");

#ifdef RIM_USETAP
	if ((args->tapfd = tun_alloc(args->tapname)) == -1) {
		perror("tun_alloc");
		fprintf(stderr, "failed to open %s\n", args->tapname);
		return -1;
	}
#endif /* RIM_USETAP */

	for (i = 0; i < MAX_QUEUED_MPAKS; i++) {
		mpak_slots[i].occupied = 0;
		mpak_slots[i].slotnum = i;
	}

	if (pthread_create(&radiotid, NULL, radio_thread, args)) {
		fprintf(stderr, "error creating radio_thread\n");
		free(args);
		return -1;
	}

	return 0;
}

int radio_setstate(int enabled)
{

	pthread_mutex_lock(&radio_lock);
	if (enabled)
		radio_info.state = 1;
	pthread_mutex_unlock(&radio_lock);

	return 0;
}

int radio_getstate(void)
{
	int ret;

	pthread_mutex_lock(&radio_lock);
	ret = radio_info.state ? 1 : 0;
	pthread_mutex_unlock(&radio_lock);

	return ret;
}

int radio_getrssi(void)
{
	int ret;

	pthread_mutex_lock(&radio_lock);
	ret = radio_info.state ? radio_info.rssi : RSSI_NO_COVERAGE;
	pthread_mutex_unlock(&radio_lock);

	return ret;
}

unsigned long radio_getman(void)
{
	unsigned long ret;

	pthread_mutex_lock(&radio_lock);
	ret = radio_info.man;
	pthread_mutex_unlock(&radio_lock);

	return ret;
}

unsigned long radio_getesn(void)
{
	unsigned long ret;

	pthread_mutex_lock(&radio_lock);
	ret = radio_info.esn;
	pthread_mutex_unlock(&radio_lock);

	return ret;
}

static void reclaimslot(struct mpak_slot *slot)
{
	if (!slot->occupied)
		return;

	fprintf(stderr, "radio: reclaiming %d\n", slot->slotnum);
	free(slot->buf);
	slot->buf = NULL;
	slot->buflen = 0;
	slot->refcount = 0;
	slot->done = 0;
	slot->occupied = 0;

	return;
}

static struct mpak_slot *findemptyslot(void)
{
	int i;

	for (i = 0; i < MAX_QUEUED_MPAKS; i++) {
		if (mpak_slots[i].occupied == 0)
			return mpak_slots+i;
		else if (mpak_slots[i].refcount <= 0) { /* reclaim it */
			reclaimslot(mpak_slots+i);
			return mpak_slots+i;
		}
	}

	return NULL;
}

static struct mpak_slot *getslotbytag(int tag)
{

	if ((tag < 0) || (tag > MAX_QUEUED_MPAKS))
		return NULL;

	if (!mpak_slots[tag].occupied)
		return NULL; /* expired */

	return mpak_slots+tag;
}

int radio_sendmpak(TASK sender, const unsigned char *buf, int buflen)
{
	int tag;
	struct mpak_slot *slot;

	if (!buf || (buflen <= 0))
		return RADIO_BAD_DATA;

	pthread_mutex_lock(&mpak_slot_lock);

	if (!(slot = findemptyslot())) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return RADIO_NO_FREE_BUFFERS;
	}

	if (!(slot->buf = malloc(buflen)))
		return RADIO_NO_FREE_BUFFERS;
	memcpy(slot->buf, buf, buflen);
	slot->buflen = buflen;
	slot->sender = sender;
	slot->direction = 0; /* transmit */
	slot->refcount = 1; /* will be decrement on actual transmit */
	slot->done = 0;
	slot->occupied = 1;

	tag = slot->slotnum;

	pthread_mutex_unlock(&mpak_slot_lock);

	return tag;
}

static void sendradiomsgrecv(int tag, int type, int hpid, struct mpak_slot *slot)
{
	rim_task_t *cur;
	MESSAGE msg;

	if (!slot)
		return;

	msg.Device = DEVICE_RADIO;
	msg.Event = MESSAGE_RECEIVED;
	msg.SubMsg = tag;
	msg.Data[0] = type;
	msg.Data[1] = (type == 0x04) ? hpid : 0;
	msg.Length = 0;
	msg.DataPtr = NULL;

	for (cur = rim_task_list; cur; cur = cur->next) {

		if (!(cur->flags & RIM_TASKFLAG_WANTSRADIO))
			continue;

		enqueuemsg(cur, &msg, RIM_TASK_INVALID);
		slot->refcount++; /* decremented when app recieves it */

	}

	return;
}

int radio_recvmpak(int type, int hpid, const unsigned char *buf, int buflen)
{
	int tag;
	struct mpak_slot *slot;

	if (!buf || (buflen <= 0))
		return -1;

	pthread_mutex_lock(&mpak_slot_lock);

	if (!(slot = findemptyslot())) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return -1;
	}

	if (!(slot->buf = malloc(buflen)))
		return -1;
	memcpy(slot->buf, buf, buflen);
	slot->buflen = buflen;
	slot->sender = RIM_TASK_INVALID;
	slot->direction = 1; /* recieve */
	slot->refcount = 0; /* will be changed later */
	slot->done = 1;
	slot->occupied = 1;

	tag = slot->slotnum;

	sendradiomsgrecv(tag, type, hpid, slot);

	pthread_mutex_unlock(&mpak_slot_lock);

	return tag;
}

int radio_retrievempak(int tag, unsigned char *destbuf, int destbuflen)
{
	struct mpak_slot *slot;
	int len;

	pthread_mutex_lock(&mpak_slot_lock);
	if (!(slot = getslotbytag(tag))) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return -1;
	}

	len = slot->buflen;

	if (destbuf && (destbuflen >= slot->buflen)) {
		memcpy(destbuf, slot->buf, 
			   (destbuflen > slot->buflen) ? destbuflen : slot->buflen);
	}

#if 1 /* XXX this is not good at all!!! */
	slot->refcount--;

	if (slot->refcount <= 0)
		reclaimslot(slot);
#endif

	pthread_mutex_unlock(&mpak_slot_lock);

	return len;
}

void radio_decrefcount(MESSAGE *msg)
{
	struct mpak_slot *slot;

	if (!msg || !(msg->Device == DEVICE_RADIO))
		return;

	if (msg->Event != MESSAGE_RECEIVED)
		return;

	pthread_mutex_lock(&mpak_slot_lock);
	if (!(slot = getslotbytag(msg->SubMsg))) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return;
	}

#if 0
	slot->refcount--;

	if (slot->refcount <= 0)
		reclaimslot(slot);
#endif

	pthread_mutex_unlock(&mpak_slot_lock);

	return;
}

static int gethpid(const unsigned char *buf, int buflen)
{
	if (buflen < 12)
		return -1;

	if (buf[7] != MPAK_HPDATA)
		return -1;

	return buf[11];
}

static int dotx_tap(struct radio_thread_args *args, struct mpak_slot *slot)
{
	unsigned char *pkt;
	int pktlen;

	pktlen = slot->buflen-12;

	fprintf(stderr, "dotx_tap:\n");
	dumpbox(slot->buf+12, slot->buflen-12);

	if (!(pkt = malloc((pktlen))))
		return 0;

	memcpy(pkt, slot->buf+12, slot->buflen-12);

	fprintf(stderr, "dotx_tap: (two_\n");
	dumpbox(pkt, pktlen);

	if (write(args->tapfd, pkt, pktlen) <= 0)
		perror("radio: dotx_tap: write");

	free(pkt);

	return 1;
}

static int dotx_rap(struct radio_thread_args *args, struct mpak_slot *slot)
{

	/* XXX fill this in */

	fprintf(stderr, "dotx_rap:\n");
	dumpbox(slot->buf, slot->buflen);

	return 1;
}

static int dotx(struct radio_thread_args *args, struct mpak_slot *slot)
{

	if ((args->tapfd != -1) && 
		(gethpid(slot->buf, slot->buflen) == RIM_TAP_HPID))
		return dotx_tap(args, slot);

	return dotx_rap(args, slot);
}

static int onetransmit(struct radio_thread_args *args)
{
	int i;
	struct mpak_slot *slot = NULL;

	pthread_mutex_lock(&mpak_slot_lock);

	for (i = 0; i < MAX_QUEUED_MPAKS; i++) {
		if (!mpak_slots[i].occupied)
			continue;
		if ((mpak_slots[i].direction == 0) && !mpak_slots[i].done) {
			slot = mpak_slots+i;
			break;
		}
	}

	if (!slot) {
		pthread_mutex_unlock(&mpak_slot_lock);
		return 0; /* nothing to do */
	}

	slot->done = dotx(args, slot);

#if 1 /* XXX this should be done later, since we need to send a status update to the sender */
	if (slot->done)
		reclaimslot(slot);
#endif

	pthread_mutex_unlock(&mpak_slot_lock);

	return 0;
}
