/* -*- Mode: ab-c -*- */

#include <rimsim.h>

static void dumptask(rim_task_t *tsk)
{

	printf("Current task:\n");
	printf("\tName: %s\n", tsk->name);
	printf("\tFlags: 0x%08lx\n", tsk->flags);
	printf("\tTask ID: %ld\n", tsk->taskid);
	printf("\tParent Task: %ld\n", tsk->parent);
	printf("\tStack size: %ld\n", tsk->stacksize);
	printf("\tRunnable: %s\n", tsk->runnable ? "yes" : "no");
	printf("\tThread ID: %ld\n", tsk->tid);

	return;
}

void sim_RimCatastrophicFailure(char *FailureMessage)
{
	printf("sim: RimCatastrophicFailure(%s)\n", FailureMessage);

	dumptask(rim_task_current);

	abort();

	return;
}
