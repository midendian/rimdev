/* -*- Mode: ab-c -*- */

#include <rimsim.h>

static void dumptask(rim_task_t *tsk)
{

	fprintf(stderr, "Current task:\n");
	fprintf(stderr, "\tName: %s\n", tsk->name);
	fprintf(stderr, "\tFlags: 0x%08lx\n", tsk->flags);
	fprintf(stderr, "\tTask ID: %ld\n", tsk->taskid);
	fprintf(stderr, "\tParent Task: %ld\n", tsk->parent);
	fprintf(stderr, "\tStack size: %ld\n", tsk->stacksize);
	fprintf(stderr, "\tRunnable: %s\n", tsk->runnable ? "yes" : "no");
	fprintf(stderr, "\tThread ID: %ld\n", tsk->tid);

	return;
}

void sim_RimCatastrophicFailure(char *FailureMessage)
{

	SIMTRACE("RimCatastrophicFailure", "%s", FailureMessage);

	dumptask(rim_task_current);

	abort();

	return;
}
