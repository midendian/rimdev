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
#include <sys/time.h>

#include <simrimos.h>

#include <pthread.h>

extern struct timeval rim_bootuptv;

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

typedef struct rim_timer_s {
	unsigned long id;
	int type; /* TIME_ONE_SHOT, TIMER_PERIODIC or TIMER_ABSOLUTE */
	unsigned long time; /* interval or absolute time */
	unsigned long lastfire;
	struct rim_timer_s *next;
} rim_timer_t;

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
	rim_timer_t *timers;
	pthread_mutex_t timerlock;
	struct rim_task_s *next;
} rim_task_t;

extern rim_task_t *rim_task_list;
extern pthread_mutex_t rim_task_list_lock;
extern rim_task_t *rim_task_current;

void debugprintf(const char *fmt, va_list ap);

rim_task_t *createtask(rim_entry_t entry, int stacksize, TASK parent, const char *name);
int inittask(rim_task_t *task);
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

int timer_set(rim_task_t *task, unsigned long id, int type, unsigned long time);
void timer_kill(rim_task_t *task, unsigned long id);

unsigned long timer_getticks(void);

#define RIM_MAXMOBITEX_BUFLEN 560
int radio_retrievempak(int tag, unsigned char *destbuf, int destbuflen);
int radio_sendmpak(TASK sender, const unsigned char *buf, int buflen);

void gui_putstring(int x, int y, const char *str, int len);
void gui_scroll(int pixels);
void gui_drawline(int type, int x1, int y1, int x2, int y2);
void gui_putbitmap(const unsigned char *bitmap, int width, int height, int destx, int desty);

#endif /* __RIMSIM_H__ */
