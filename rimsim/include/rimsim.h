/* -*- Mode: ab-c -*- */

#ifndef __RIMSIM_H__
#define __RIMSIM_H__

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdarg.h>

#include <simrimos.h>

#include <pthread.h>

#define RIM_MAXAPPNAMELEN 64

#define RIM_TASKFLAG_NONE             0x00000000
#define RIM_TASKFLAG_ENABLEFOREGROUND 0x00000001
#define RIM_TASKFLAG_ICONPRESENT      0x00000002
#define RIM_TASKFLAG_NOTYETRUN        0x00000004
#define RIM_TASKFLAG_WANTSRADIO       0x00000008 /* called RadioRegister() */

#define RIM_TASK_INVALID     0 /* defined by ABI */
#define RIM_TASK_NOPARENT    1
#define RIM_TASK_FIRSTID   100

typedef struct rim_msg_s {
	TASK poster;
	MESSAGE *msg;
	struct rim_msg_s *next;
} rim_msg_t;

typedef void (*rim_entry_t)(void) __attribute__((__cdecl__));
typedef struct rim_task_s {
	char name[RIM_MAXAPPNAMELEN];
	unsigned long flags;
	rim_entry_t entry;
	BitMap icon;
	TASK parent;
	TASK taskid;
	unsigned long stacksize;
#if 0
	unsigned char *stackbase;
	unsigned long esp; /* current stack pointer */
	unsigned long eip; /* current instruction pointer */
#else
	pthread_t tid;
	int runnable;
	pthread_cond_t execcond;
	pthread_mutex_t execlock;
	rim_msg_t *msgqueue;
	pthread_mutex_t msglock;
#endif
	struct rim_task_s *next;
} rim_task_t;

extern rim_task_t *rim_task_list;
extern pthread_mutex_t rim_task_list_lock;
extern rim_task_t *rim_task_current;

TASK createtask(rim_entry_t entry, int stacksize, TASK parent, const char *name);
void schedule(void);
void firstschedule(void);

int sendmessage(MESSAGE *msg, TASK poster);
int sendmessageto(rim_task_t *target, MESSAGE *msg, TASK poster);
MESSAGE *getmessage(TASK *taskret);
void freemessage(MESSAGE *msg);

int radio_setstate(int enabled);
int radio_getstate(void);
int radio_getrssi(void);
unsigned long radio_getman(void);
unsigned long radio_getesn(void);

#define RIM_MAXMOBITEX_BUFLEN 560
int radio_retrievempak(int tag, unsigned char *destbuf, int destbuflen);
int radio_sendmpak(TASK sender, const unsigned char *buf, int buflen);

#endif /* __RIMSIM_H__ */
