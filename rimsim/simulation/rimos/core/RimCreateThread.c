/* -*- Mode: ab-c -*- */

#include <rimsim.h>

/*
 * Create a new thread (but do not run it (yet)).
 *
 * This thread uses the same data space as the parent, but with
 * a different stack (with the specified size).
 *
 * You may wonder why the data space sharing is never mentioned. This
 * is because it is all done implicitly by the loader, where it should
 * be.  Only the loader can allocate .data and bring them in from
 * the object file.  RimCreateThread can only add a task that executes
 * the same code.
 *
 * The main rimsim startup looks basically identical to what follows.
 * Isn't that neat.
 *
 */
TASK sim_RimCreateThread(void (*entry)(void), DWORD stacksize)
{
	rim_task_t *tsk;

	SIMTRACE("RimCreateThread", "%p, %ld", entry, stacksize);

	if (!(tsk = createtask((rim_entry_t)entry, stacksize, rim_task_current->taskid, rim_task_current->name)))
		return RIM_TASK_INVALID;

	inittask(tsk);

	return tsk->taskid;
}
