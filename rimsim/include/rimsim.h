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

#define RIM_MAXAPPNAMELEN 64

#define RIM_TASKFLAG_NONE             0x00000000
#define RIM_TASKFLAG_ENABLEFOREGROUND 0x00000001
#define RIM_TASKFLAG_ICONPRESENT      0x00000002

#define RIM_TASK_INVALID     0 /* defined by ABI */
#define RIM_TASK_NOPARENT    1
#define RIM_TASK_FIRSTID   100

typedef void (*rim_entry_t)(void) __attribute__((__cdecl__));
typedef struct rim_task_s {
	char name[RIM_MAXAPPNAMELEN];
	unsigned long flags;
	rim_entry_t entry;
	BitMap icon;
	TASK parent;
	TASK taskid;
	unsigned char *stackbase;
	unsigned long stacksize;
	unsigned long esp; /* current stack pointer */
	unsigned long eip; /* current instruction pointer */
	struct rim_task_s *next;
} rim_task_t;

extern rim_task_t *rim_task_list;
extern rim_task_t *rim_task_current;

TASK createtask(rim_entry_t entry, int stacksize, TASK parent, const char *name);
void schedule(void);
void firstschedule(void);

#endif /* __RIMSIM_H__ */
