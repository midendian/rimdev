/* -*- Mode: ab-c -*- */

/*
 * The process scheduler. 
 *
 * Several things need to get implemented before this can 
 * actually be a scheduler; namely:
 *      - Seperate task stacks (this appears to work now)
 *      - Saving of EIP values for when a task enters the scheduler
 *      - Condition waits so that tasks can escape the 
 *          scheduler (ie, RimGetMessage)
 *
 *
 */

#include <rimsim.h>

rim_task_t *rim_task_list = NULL;
rim_task_t *rim_task_current = NULL;
static TASK rim_nexttaskid = RIM_TASK_FIRSTID;

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

	/* If it has no parent, automatically give it foreground access. */
	if (newtask->parent == RIM_TASK_NOPARENT)
		newtask->flags |= RIM_TASKFLAG_ENABLEFOREGROUND;

	if (!(newtask->stackbase = malloc(newtask->stacksize))) {
		free(newtask);
		return RIM_TASK_INVALID;
	}

	/* ia32 stack grow down */
	newtask->esp = (unsigned long)newtask->stackbase + newtask->stacksize;

	/* starting EIP is same as entry point */
	newtask->eip = (unsigned long)newtask->entry;

	fprintf(stderr, "createtask: %s / 0x%08lx / %p / %ld / %ld / %ld@0x%08lx-0x%08lx / 0x%08lx\n", newtask->name, newtask->flags, newtask->entry, newtask->parent, newtask->taskid, newtask->stacksize, (unsigned long)newtask->stackbase, newtask->esp, newtask->eip);

	newtask->next = rim_task_list;
	rim_task_list = newtask;

	/* If we haven't started running a task yet, set the first one runnable */
	if (!rim_task_current)
		rim_task_current = rim_task_list;

	return newtask->taskid;
}

static unsigned long ourstackptr = 0;

/*
 * This is the one called by main().  It has to be special relative
 * to what the OS calls because the incoming context is not part of
 * a RIM task.
 */
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
#if 0 /* this actually works right now */
	rim_task_current->entry();
#endif

	return;
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
#if 0
	__asm__ __volatile__ (
						  "movl 4(%%esp), %0;"
						  "movl %%esp, %1;"
						  :"=r"(rim_task_current->eip), 
						  "=r"(rim_task_current->esp));
#else
	__asm__ __volatile__ (
						  "movl 4(%%esp), %0;"

#if 1
						  /* Remove ourselves */
						  "popl %%eax;"
						  "popl %%eax;"
#endif

						  /* Save context */
#if 0
						  "pushl %%eax;"
						  "pushl %%ebx;"
						  "pushl %%ecx;"
						  "pushl %%edx;"
						  "pushl %%esi;"
						  "pushl %%edi;"
						  "pushl %%ebp;"
#else
						  "pusha;"
#endif
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
#endif

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
#if 0
							  "popl %%ebp;" /* restore their integer context */
							  "popl %%edi;"
							  "popl %%esi;"
							  "popl %%edx;"
							  "popl %%ecx;"
							  "popl %%ebx;"
							  "popl %%eax;"
#else
							  "popa;"
							  "movl %0, %%esp;" /* restore their stack */
							  "movl %1, %%ebp;"
#endif
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
