/* -*- Mode: ab-c -*- */

/*
 * RIM Application Loader.  Or something.
 *
 * So far, its read only. And you really have to know what you're doing.
 * All the linking and things have to be done on the host side, the
 * results getting written to the magic spots in the RIMOS image.  Someone
 * should figure out how to do that.
 *
 * I really just pursued this so I could read abitrary parts of flash.  The
 * programmer.exe that comes with the BB2.0 SDK runs fine under wine.  Use
 * that to load apps.
 *
 * What would be more useful is having higher-level sync protocol implemented,
 * for backing up messages, the databases, etc.  But, ironically, thats a
 * much more complicated protocol than this one.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MODEMDEVICE "/dev/ttyS0"

static void dumpbox(unsigned long addr, unsigned char *buf, int len)
{
	char z[256];
	int i, y, x;

	for (i = 0, x = 0; i < len; ) {
		x = snprintf(z, sizeof(z), "0x%08lx:  ", addr+i);
		for (y = 0; y < 8; y++) {
			if (i < len) {
				x += snprintf(z+x, sizeof(z)-x, "%02x", *(buf+i));
				i++;
			} else
				break;
		}
		printf("%s\n", z);
		fflush(stdout);
	}

	return;
}

static int openport(char *file)
{
	struct termios oldtio,newtio;
	int fd;

	fprintf(stderr, "opening...\n");
	if ((fd = open(file, O_RDWR | O_NOCTTY)) < 0) {
		perror(file);
		return -1;
	}
	fprintf(stderr, "...opened\n");
  
	tcgetattr(fd,&oldtio); /* save current port settings */
  
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = 0;
	newtio.c_oflag = 0;
  
	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
  
	newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;   /* blocking read until 0 chars received */
  
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);

	return fd;
}

static int getflags(int fd, int v)
{
	int flags;

	if (ioctl(fd, TIOCMGET, &flags) < 0)
		perror("ioctl");
  
	if (v) {
		fprintf(stderr, "Flags: %08x = %s %s %s %s %s %s %s %s %s\n",
				flags,
				(flags & TIOCM_LE)?"LE":"",
				(flags & TIOCM_DTR)?"DTR":"",
				(flags & TIOCM_RTS)?"RTS":"",
				(flags & TIOCM_ST)?"ST":"",
				(flags & TIOCM_SR)?"SR":"",
				(flags & TIOCM_CTS)?"CTS":"",
				(flags & TIOCM_CAR)?"CAR":"",
				(flags & TIOCM_RNG)?"RNG":"",
				(flags & TIOCM_DSR)?"DSR":"");
	}

	return flags;
}

static int setcrtscts(int fd, int val)
{
	struct termios tio;
  
	if (tcgetattr(fd, &tio) == -1)
		return -1;

	if (val)
		tio.c_cflag |= CRTSCTS;
	else
		tio.c_cflag &= ~CRTSCTS;

	return tcsetattr(fd, TCSANOW, &tio);
}

static int setdtr(int fd, int val)
{
	int flags;

	if (ioctl(fd, TIOCMGET, &flags) == -1)
		perror("ioctl");

	if (val)
		flags |= TIOCM_DTR;
	else
		flags &= ~TIOCM_DTR;

	fprintf(stderr, "setdtr: new val = %x\n", flags);

	return ioctl(fd, TIOCMSET, &flags);
}

/*
 * To get it to listen to us:
 *
 *   - Open with no flow control, 8N1 at 9600
 *   - Raise DTR
 *   - Poll for DSR to come up
 *   - Clear DTR
 *   - Raise DTR
 *   - Make sure DSR is now cleared
 *   - Reenable CRTSCTS flow control
 *
 */
static int setlines(int fd)
{
	int flags;
	time_t endtime;

	flags = getflags(fd, 1);

	setcrtscts(fd, 0);

	/* These two lines aren't required, but seem to help it along a bit */
	setdtr(fd, 0);
	usleep(1000);

	setdtr(fd, 1);

	for (endtime = time(NULL) + 5; time(NULL) <= endtime; ) {
		usleep(100000);
		flags = getflags(fd, 1);
		if (flags & TIOCM_DSR) {
			fprintf(stderr, " --- DSR came up, we're good\n");
			break;
		}
	}

	setdtr(fd, 0); /* clear this, error or not */

	if (!(flags & TIOCM_DSR)) {
		fprintf(stderr, " --- DSR never came up\n");
		return -1; /* never came up... */
	}

	setdtr(fd, 1);

	for (endtime = time(NULL) + 5; time(NULL) <= endtime; ) {
		flags = getflags(fd, 1);
		if (!(flags & TIOCM_DSR)) {
			fprintf(stderr, " --- DSR went down...good\n");
			break;
		}
	}

	if (flags & TIOCM_DSR)
		fprintf(stderr, " -- WARNING DSR never went down\a\n");

	setcrtscts(fd, 1);

	return 0;
}

#define LOADER_NOP 0x00
#define LOADER_CMD 0x01
#define LOADER_END 0x04
#define LOADER_ACK 0x06

/* 
 * Commands go like this: (example is reading mem)
 *
 *  Host:
 *    <LOADER_CMD> <LOADER_CMD_READMEM> <LOADER_END>
 *  Pager:
 *    <LOADER_ACK>
 *  Host:
 *    <32bit address> <16bit length>
 *  Pager:
 *    <LOADER_ACK> <data> <16bit crc>
 *
 */
#define LOADER_CMD_SETBAUD ('B')
#define LOADER_CMD_RAMSIZE ('r')
#define LOADER_CMD_READMEM ('R')
#define LOADER_CMD_WRITEMEM ('W')
#define LOADER_CMD_EXIT ('X')

/*
 * The usleeps in here are fairly critical, unfortunatly.
 *
 */
static int readresponse(int fd, unsigned char *destbuf, int len)
{
	time_t endtime;
	int ret, i;

	endtime = time(NULL) + len + 5; /* XXX assume one sec per byte... heh */

	usleep(2000);

	/*
	 * Select refuses to work, so I'll live with polling for now. XXX
	 */
	for (i = 0; (i < len) && (time(NULL) <= endtime); ) {
		if ((ret = read(fd, destbuf+i, len-i)) == -1) {
			perror("read");
			return -1;
		}
		i += ret;
	}

	usleep(2000);

	fprintf(stderr, "read %d bytes\n", i);

	return i;
}

/* 
 * destbuf must be (len+2) in length (crc at end)
 *
 * This assumes we're running on a little-endian host (XXX)
 */
static int reqmem(int fd, unsigned long addr, unsigned short len, unsigned char *destbuf)
{
	unsigned char cmd[] = {LOADER_CMD, LOADER_CMD_READMEM, LOADER_END};
	unsigned char ack;

	if (write(fd, cmd, sizeof(cmd)) < sizeof(cmd)) {
		perror("reqmem: write");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	if (ack != LOADER_ACK) {
		fprintf(stderr, "reqmem: unexpected response 0x%02x (req)\n", ack);
		return -1;
	}

	if (write(fd, (unsigned char *)&addr, 4) < 4) {
		perror("reqmem: write2");
		return -1;
	}

	if (write(fd, (unsigned char *)&len, 2) < 2) {
		perror("reqmem: write2");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	if (ack != LOADER_ACK) {
		fprintf(stderr, "reqmem: unexpected response 0x%02x (addr)\n", ack);
		return -2; /* invalid address */
	}

	return readresponse(fd, destbuf, len+2);
}

/*
 * Tell the pager to increase the bit rate to 115200, and tell
 * termios, too.
 */
static int maxbaud(int fd)
{
	unsigned char cmd[] = {LOADER_CMD, LOADER_CMD_SETBAUD, LOADER_END};
	unsigned char ack;
	unsigned long newbaud = 115200;
	struct termios tio;
  
	if (write(fd, cmd, sizeof(cmd)) < sizeof(cmd)) {
		perror("maxbaud: write");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	if (ack != 0x04) { /* not sure what this is */
		fprintf(stderr, "maxbaud: unexpected response 0x%02x (req)\n", ack);
		return -1;
	}

	if (write(fd, (unsigned char *)&newbaud, 4) < 4) {
		perror("maxbaud: write2");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	if (ack != LOADER_ACK) {
		fprintf(stderr, "maxbaud: unexpected response 0x%02x (val)\n", ack);
		return -2; /* invalid address */
	}

	if (tcgetattr(fd, &tio) == -1)
		return -1;

	tio.c_cflag &= ~B9600;
	tio.c_cflag |= B115200;

	return tcsetattr(fd, TCSANOW, &tio);
}

static int getramsize(int fd)
{
	static const unsigned char cmd[] = {LOADER_CMD, LOADER_CMD_RAMSIZE, LOADER_END};
	unsigned char ack;
	unsigned long size;

	if (write(fd, cmd, sizeof(cmd)) < sizeof(cmd)) {
		perror("getramsize: write");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	if (readresponse(fd, (unsigned char *)&size, 4) != 4)
		return -1;

	fprintf(stderr, "pager has %ld bytes of RAM\n", size);

	return 0;
}

static int stoploader(int fd)
{
	static const unsigned char cmd[] = {LOADER_CMD, LOADER_CMD_EXIT, LOADER_END};
	unsigned char ack;

	if (write(fd, cmd, sizeof(cmd)) < sizeof(cmd)) {
		perror("reqmem: write");
		return -1;
	}

	if (readresponse(fd, &ack, 1) != 1)
		return -1;

	fprintf(stderr, "loader connection closed\n");

	return 0;
}

static int startloader(int fd)
{
	static const unsigned char clearchan[] = {LOADER_NOP};
	static const unsigned char attnreq[] = {0x0e, 0xaa, 0x0f, 0xf0};
	int ret;
	unsigned char b = 0;

	/*
	 * First write a NOP to kill any pending commands.
	 */
	if (write(fd, clearchan, sizeof(clearchan)) == -1) {
		perror("startloader: write(nop)");
		return -1;
	}

	/*
	 * Request the attention of the pager until we get an ACK.
	 *
	 * XXX this should actually repeat instead of giving up.
	 */

	/*
	 * Write/sleep/write.  ***THE SLEEP IS CRITICAL***
	 */
	if ((ret = write(fd, attnreq, sizeof(attnreq))) < sizeof(attnreq)) {
		perror("startloader: write(nop)");
		return -1;
	}

	usleep(2000);

	if ((ret = write(fd, attnreq, sizeof(attnreq))) < sizeof(attnreq)) {
		perror("startloader: write(nop)");
		return -1;
	}

	tcdrain(fd); /* just to make sure */

	fprintf(stderr, "going into readresponse\n");
	if (readresponse(fd, &b, 1) < 1) {
		fprintf(stderr, "startloader: readresponse failed\n");
		return -1;
	}

	if (b != LOADER_ACK) {
		fprintf(stderr, "got unexpected command 0x%02x\n", b);
		return -1;
	}

	fprintf(stderr, "got ack from attn\n");

	return 0;
}

int main(int argc, char **argv)
{
	int fd;

	if ((fd = openport(MODEMDEVICE)) == -1)
		return -1;

	setlines(fd);

	if ((startloader(fd)) == -1) {
		fprintf(stderr, "going into close\n");
		close(fd);
		fprintf(stderr, "closed\n");
		return -1;
	}

	if (maxbaud(fd) == -1)
		fprintf(stderr, "maxbaud failed\n");

	if (0) {
		unsigned char buf[0xcc+2];

		if (reqmem(fd, 0x03ffc020, 0xcc, buf) == -1)
			fprintf(stderr, "reqmem failed\n");
		else
			dumpbox(0x03ffc020, buf, 0xcc+2);
	}

	if (0) {
		unsigned char buf[0x0004+2];

		if (reqmem(fd, 0x03ff403c, 0x0004, buf) == -1)
			fprintf(stderr, "reqmem failed\n");
		else
			dumpbox(0x03ff403c, buf, 0x0004+2);
	}

	/*
	 * The flash rom is mapped from 0x03b10000 to 0x03ffffff.
	 * However, when running on the RIM you can dereference down to
	 * 0x03b00000.  I can't explain that. if you do the math, it
	 * must go all the way down to 0x03b00000 to add up to 5M.
	 *
	 */
	if (1) { /* this dumps all the upper stuff */
		FILE *f;
		unsigned char buf[256+2];
#if 0
		unsigned long curaddr = 0x03ff0000;
		unsigned long stopaddr =0x04000000;
#else
		unsigned long curaddr = 0x00000000;
		unsigned long stopaddr =0x00100000;
#endif
		int i = 0;
		int ret;

		f = fopen("./dump2.bin", "w");
		while (curaddr < stopaddr) {
			fprintf(stderr, "doing pass %d (0x%08lx-0x%08lx)\n", i, curaddr, curaddr+256);
			if ((ret = reqmem(fd, curaddr, 256, buf)) == -1) {
				fprintf(stderr, "reqmem failed\n");
				break;
			} else if (ret == -2) {
				memset(buf, 0x42, 256);
				fwrite(buf, 256, 1, f);
				fprintf(stderr, "0x%08lx is not mapped\n", curaddr);
			} else {
				dumpbox(curaddr, buf, 256);
				fwrite(buf, 256, 1, f);
			}
			i++;
			curaddr += 256;
		}
		fclose(f);
	}

	getramsize(fd);

	stoploader(fd);

	fprintf(stderr, "going into close\n");
	close(fd);
	fprintf(stderr, "closed\n");

	return 0;
}
