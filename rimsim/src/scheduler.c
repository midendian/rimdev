/* -*- Mode: ab-c -*- */

/*
 * The process scheduler. 
 *
 * So originally I was brave, and started doing this The Real Way.
 *
 * Then I got bored of typing '__asm__', so I'm cheating and doing
 * it the Real Easy Way.
 *
 * Each task gets its own (p)thread.  In this scheme, the scheduler's
 * role is to _keep_ threads from getting scheduled instead of actually
 * making them go.  Since the target emulation is that of a cooperative
 * multitasking OS, only one thread can run at a time, and even then, 
 * only when another thread specifically yields.  Pthreads doesn't let
 * us have such control over things, so we enforce it using conditioned
 * waits.
 *
 * Every thread gets created and enters taskentry(), at which point
 * it immediatly gets put to sleep, waiting for ->running to become
 * nonzero, at which point it begins its initial execution.  [This 
 * isn't exactly how RIMOS seems to do it.  For it, task creation
 * and initial execution seem to be one step.  I think thats dumb
 * and I'm not going to do that.  Its not critical, since
 * all apps are written to handle the opposite case, there should
 * be no problem in being ready too soon.  That is, every RIM app
 * should begin with a RimTaskYield().]  Once the execution begins,
 * the first call to one of the blocking syscalls (RimTaskYield or
 * a RimGetMessage varient) results in a call to schedule(), where
 * control is yielded to all tasks waiting to run; eventually the
 * list will be exhausted and the original waiting task will get run
 * again.
 *
 * The definition of RimGetMessage is fairly clear: the task will
 * wake up when a message arrives for it to return.  Under no
 * circumstances will RimGetMessage fail to return a message.
 *
 * The definition of RimTaskYield is by contrast, quite vague. I have
 * inferred that it will run every task that is waiting to run, not
 * just one task and then return (like one might think if they're 
 * coming from a traditional kernel).  I have inferred this mainly
 * because the documentation says to call RimTaskYield immediatly, 
 * before any calls are made to other tasks.  Since it is only
 * called once, and you could make calls to anything afterward, it must
 * run them all.
 *
 * This code is all nice and straightforward.  Until I go back and do it
 * the real way, and removing pthreads.  And doing full context switches
 * in here.
 *
 */

#include <rimsim.h>

#undef DEBUG_SCHEDULE

rim_task_t *rim_task_list = NULL;
pthread_mutex_t rim_task_list_lock = PTHREAD_MUTEX_INITIALIZER;
rim_task_t *rim_task_current = NULL;
static TASK rim_nexttaskid = RIM_TASK_FIRSTID;
static time_t lastslicestart = 0;

TASK createtask(rim_entry_t entry, int stacksize, TASK parent, const char *name)
{
	rim_task_t *newtask;

	if (!entry || (stacksize < 2000))
		return RIM_TASK_INVALID;

	if (!(newtask = malloc(sizeof(rim_task_t))))
		return RIM_TASK_INVALID;
	memset(newtask, 0, sizeof(rim_task_t));

	newtask->taskid = rim_nexttaskid++;
	newtask->parent = (parent >= RIM_TASK_FIRSTID) ? parent : RIM_TASK_NOPARENT;
	newtask->flags = RIM_TASKFLAG_NOTYETRUN; /* set until first task switch */
	if (name)
		strncpy(newtask->name, name, RIM_MAXAPPNAMELEN);
	newtask->entry = entry;
	newtask->stacksize = stacksize;

	newtask->runnable = 0;
	pthread_cond_init(&newtask->execcond, NULL);
	pthread_mutex_init(&newtask->execlock, NULL);

	newtask->msgqueue = NULL;
	pthread_mutex_init(&newtask->msglock, NULL);

	newtask->timers = NULL;
	pthread_mutex_init(&newtask->timerlock, NULL);

	/* If it has no parent, automatically give it foreground access. */
	if (newtask->parent == RIM_TASK_NOPARENT)
		newtask->flags |= RIM_TASKFLAG_ENABLEFOREGROUND;

	pthread_mutex_lock(&rim_task_list_lock);

	newtask->next = rim_task_list;
	rim_task_list = newtask;

	/* If we haven't started running a task yet, set the first one runnable */
	if (!rim_task_current)
		rim_task_current = rim_task_list;

	pthread_mutex_unlock(&rim_task_list_lock);

	return newtask->taskid;
}

/*
 * Wake up the task x. Its up to the context to make 
 * sure no other task is running.
 */
#define TASK_WAKEUP(x) { \
	pthread_mutex_lock(&x->execlock); \
	x->runnable = 1; \
	pthread_cond_signal(&x->execcond); \
	pthread_mutex_unlock(&x->execlock); \
}

/*
 * Put x to sleep.  
 */
#define TASK_PUTTOSLEEP(x) { \
	pthread_mutex_lock(&x->execlock); \
	x->runnable = 0; \
	pthread_mutex_unlock(&x->execlock); \
}

/*
 * Used after PUTTOSLEEP; waits for x to be set runnable
 * again.
 */
#define TASK_WAITTORUN(x) { \
	pthread_mutex_lock(&x->execlock); \
	while (!x->runnable) \
		pthread_cond_wait(&x->execcond, &x->execlock); \
	pthread_mutex_unlock(&x->execlock); \
}

static void *taskentry(void *arg)
{
	rim_task_t *self = (rim_task_t *)arg;

#ifdef DEBUG_SCHEDULE
	fprintf(stderr, "taskentry for %ld\n", self->taskid);
#endif

	/* Wait until we're set runnable for the first time. */
	TASK_WAITTORUN(self);

	self->flags &= ~RIM_TASKFLAG_NOTYETRUN;

	/* Do it! */
	self->entry();

#ifdef DEBUG_SCHEDULE
	fprintf(stderr, "task %ld died!\n", self->taskid);
#endif

	return NULL;
}
    
/*
 * This is the one called by main().  It has to be special relative
 * to what the OS calls because the incoming context is not part of
 * a RIM task.
 *
 * It is also responsible for creating the threads for each task. After
 * the first task is woken up, this thread just sits around and waits.
 * Therefore, the watchpuppy is implemented here.
 *
 */
void firstschedule(void)
{
	rim_task_t *cur;

	for (cur = rim_task_list; cur; cur = cur->next) {
		if (pthread_create(&cur->tid, NULL, taskentry, cur)) {
			fprintf(stderr, "firstschedule: unable to create thread for %ld\n", cur->taskid);
			return;
		}
	}

	/* Wake up first task */
	TASK_WAKEUP(rim_task_current);

	time(&lastslicestart);

	for (;;) {

#ifdef DEBUG_SCHEDULE
		fprintf(stderr, "main thread sleeping...\n");
#endif
		sleep(1);

		if ((time(NULL) - lastslicestart) > 5)
			sim_RimCatastrophicFailure("Error 96: watchpuppy hath barked");

		/* XXX should really lock the task list... */
		for (cur = rim_task_list; cur; cur = cur->next) {
			rim_timer_t *t;

			/* XXX actually support things other than PERIODIC */
			pthread_mutex_lock(&cur->timerlock);
			for (t = cur->timers; t; t = t->next) {

				if (t->type == TIMER_PERIODIC) {

					if ((timer_getticks() - t->lastfire) >= t->time) {
						MESSAGE msg;

						msg.Device = DEVICE_TIMER;
						msg.Event = t->id;

						sendmessageto(cur, &msg, RIM_TASK_INVALID);

						t->lastfire = sim_RimGetTicks();
					}

				}
			}
			pthread_mutex_unlock(&cur->timerlock);
		}

	}

	return;
}

void schedule(void)
{
	rim_task_t *caller;

	caller = rim_task_current;

#ifdef DEBUG_SCHEDULE
	fprintf(stderr, "putting %ld to sleep\n", caller->taskid);
#endif
	TASK_PUTTOSLEEP(caller);

	/* 
	 * Switch to the next task 
	 *
	 * XXX This should only switch to a task that has pending
	 * messages (other than yourself).  (RimTaskYield should
	 * return immediatly if there are no tasks waiting for
	 * messages.)
	 */
	if (rim_task_current->next)
		rim_task_current = rim_task_current->next;
	else
		rim_task_current = rim_task_list;
	
#ifdef DEBUG_SCHEDULE
	fprintf(stderr, "waking up %ld\n", rim_task_current->taskid);
#endif

	TASK_WAKEUP(rim_task_current);

	time(&lastslicestart);

#ifdef DEBUG_SCHEDULE
	fprintf(stderr, "starting %ld back up\n", caller->taskid);
#endif

	/* Wait until we're set runable again */
	TASK_WAITTORUN(caller);

	/* At this point we can assume rim_task_current == caller again */
	if (rim_task_current != caller)
		abort();

	return;
}

static rim_timer_t *findtimer(rim_task_t *task, unsigned long id)
{
	rim_timer_t *cur;

	for (cur = task->timers; cur; cur = cur->next) {
		if (cur->id == id)
			return cur;
	}

	return NULL;
}

int timer_set(rim_task_t *task, unsigned long id, int type, unsigned long time)
{
	rim_timer_t *t;

	pthread_mutex_lock(&task->timerlock);
	if (findtimer(task, id)) {
		pthread_mutex_unlock(&task->timerlock);
		return -1;
	}

	if (!(t = malloc(sizeof(rim_timer_t)))) {
		pthread_mutex_unlock(&task->timerlock);
		return -1;
	}

	t->id = id;
	t->type = type;
	t->time = time;
	t->lastfire = 0;

	t->next = task->timers;
	task->timers = t;

	pthread_mutex_unlock(&task->timerlock);

	return 0;
}

void timer_kill(rim_task_t *task, unsigned long id)
{
	rim_timer_t *cur, **prev;

	pthread_mutex_lock(&task->timerlock);
	for (prev = &task->timers; (cur = *prev); ) {

		if (cur->id == id) {
			*prev = cur->next;
			free(cur);
			return;
		}

		prev = &cur->next;
	}
	pthread_mutex_unlock(&task->timerlock);

	return;
}

unsigned long timer_getticks(void)
{
	struct timeval tv;
	struct timezone tz;
	struct timeval diff;

	if (gettimeofday(&tv, &tz) == -1)
		return 0; /* erm */

	timersub(&tv, &rim_bootuptv, &diff);

	return (diff.tv_sec * 100) + (diff.tv_usec / 10000);
}

#if 0 /* old non-threads stuff */

TASK createtask(void)
{
	if (!(newtask->stackbase = malloc(newtask->stacksize))) {
		free(newtask);
		return RIM_TASK_INVALID;
	}

	/* ia32 stack grow down */
	newtask->esp = (unsigned long)newtask->stackbase + newtask->stacksize;

	/* starting EIP is same as entry point */
	newtask->eip = (unsigned long)newtask->entry;

	fprintf(stderr, "createtask: %s / 0x%08lx / %p / %ld / %ld / %ld@0x%08lx-0x%08lx / 0x%08lx\n", newtask->name, newtask->flags, newtask->entry, newtask->parent, newtask->taskid, newtask->stacksize, (unsigned long)newtask->stackbase, newtask->esp, newtask->eip);
}

void firstschedule(void)
{
	unsigned long stackptr;
	unsigned long instrptr;

	fprintf(stderr, "firstschedule!\n");

	asm ("mov %%esp, %0;"
		 :"=r"(stackptr));

	fprintf(stderr, "firstschedule: our stack is at 0x%08lx\n", stackptr);

	ourstackptr = stackptr;

#if 0
	/*
	 * I leave this here as an example of how to retrieve the current 
	 * EIP value.  This works by initiating a 'call' and then returning
	 * the value that the processor stores as the caller's EIP.  That
	 * value will be the address of the instruction after the call.
	 *
	 * This is unneeded here because we don't need the current EIP, we
	 * need the pointer to instruction after the call that called us.
	 * Luckily that is much easier to get (and is down right below this).
	 *
	 * Oh, and this really fucks up gdb.
	 *
	 */
	asm volatile (
				  "call argh;"
				  "mov %%eax, %0;" /* caller EIP is returned in eax */
				  "jmp argh2;"
				  "argh:"
				  "  push %%ebp;"
				  "  mov 4(%%esp), %%eax;" /* pull off caller EIP */
				  "  mov %%esp, %%ebp;"
				  "  leave;" /* do this "properly" */
				  "  ret;"
				  "argh2:" /* fall out of asm block */
				  :"=r"(instrptr)
				  :
				  : "%eax");
#endif

	/* If all you need to know is your caller's EIP, this is easy. */
	__asm__ __volatile__ (
						  "mov 4(%%ebp), %0;"
						  :"=r"(instrptr));

	fprintf(stderr, "firstschedule: instr: our EIP is 0x%08lx\n", instrptr);

	fprintf(stderr, "firstschedule: running task: esp = 0x%08lx, eip = 0x%08lx\n", rim_task_current->esp, rim_task_current->eip);

	/* 
	 * Go ahead and jump to the first application's entry point, after 
	 * changing the stacks 
	 *
	 * Its probably not important, but note that the variables that
	 * the ESP and EIP are pulled out of are on the heap, not on the
	 * stack (which will get pulled out from under it if gcc happens
	 * to use esp-relative addresses on the movs).
	 *
	 * Since initial EIP's for RIM applications are functions (void foo(void)),
	 * it is valid for this first entry to be a call and not a jmp.  However,
	 * since the entry points take nothing, return nothing, and never return,
	 * there is no difference between jmp and call in this context (other
	 * than the fact that the call will use up 8 extra bytes of stack
	 * space for storing the calling EIP and CS).
	 *
	 */
	__asm__ __volatile__ (
						  "mov %0, %%esp;"
						  "mov %0, %%ebp;" /* XXX is this necessary? */
						  "mov %1, %%eax;"
						  "call %%eax;" /* initially a call, switch will be jmp */
						  :
						  :"r"(rim_task_current->esp),
						  "r"(rim_task_current->eip)
						  );
}

/* XXX I haven't decided how I want to do this yet. */
/*
 * This as a function is completely unworkable because gcc
 * generates code that uses the registers before we can save
 * the context.  I suppose the context needs to be saved
 * before this function is entered.
 */
void schedule(void)
{

	/*
	 * Now comes the fun part.
	 */
	
	/* 
	 * Save the caller's context. 
	 *
	 * Note that after pushing the context onto the caller's stack,
	 * the stack pointer is restored to our master stack.  This is
	 * so that all the libc calls made within schedule() are done with
	 * the stack that libc gave us, not the one we allocated for the 
	 * current task.  
	 */
	__asm__ __volatile__ (
						  "movl 4(%%esp), %0;"

						  /* Remove ourselves */
						  "popl %%eax;"
						  "popl %%eax;"

						  /* Save context */
						  "pushl %%eax;"
						  "pushl %%ebx;"
						  "pushl %%ecx;"
						  "pushl %%edx;"
						  "pushl %%esi;"
						  "pushl %%edi;"
						  "pushl %%ebp;"
						  "movl %%esp, %1;"

						  "int3;"

						  /* Restore our stack pointer */
						  "movl %2, %%esp;"
						  "movl %2, %%ebp;"

						  :
						  "=r"(rim_task_current->eip),
						  "=r"(rim_task_current->esp)
						  :
						  "r"(ourstackptr)
						  );

	/* The stack is now in a defined state, so unflag this. */
	rim_task_current->flags &= ~RIM_TASKFLAG_NOTYETRUN;

	fprintf(stderr, "schedule: %ld/%s -> %ld/%s\n", 
			rim_task_current->taskid, rim_task_current->name, 
			rim_task_current->next?rim_task_current->next->taskid:rim_task_list->taskid, 
			rim_task_current->next?rim_task_current->next->name:rim_task_list->name);


	fprintf(stderr, "schedule: saved registers: 0x%08lx, 0x%08lx\n",
			rim_task_current->eip, rim_task_current->esp);

	/* Switch to the next task */
	if (rim_task_current->next)
		rim_task_current = rim_task_current->next;
	else
		rim_task_current = rim_task_list;

	if (rim_task_current->flags & RIM_TASKFLAG_NOTYETRUN) {
		/* Change stacks and jump to new task */
		__asm__ __volatile__ (
							  "movl %0, %%esp;"
							  "movl %1, %%ebp;"
							  "jmpl %2;"
							  :
							  :"r"(rim_task_current->esp),
							  "r"(rim_task_current->stackbase),
							  "r"(rim_task_current->eip));
	} else {
		__asm__ __volatile__ (
							  "movl %0, %%esp;" /* restore their stack */
							  "movl %1, %%ebp;"
							  "popl %%ebp;" /* restore their integer context */
							  "popl %%edi;"
							  "popl %%esi;"
							  "popl %%edx;"
							  "popl %%ecx;"
							  "popl %%ebx;"
							  "popl %%eax;"
							  "int3;"
							  "jmp %1;"
							  :
							  "=r"(rim_task_current->esp),
							  "=r"(rim_task_current->stackbase)
							  :
							  "r"(rim_task_current->eip)
							  );
							  
	}

	/* Never make it here */

	return;
}
#endif /* 0 */
