/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RadioDeregister(void)
{
	printf("sim: RadioDeregister()\n");

	rim_task_current->flags &= ~RIM_TASKFLAG_WANTSRADIO;

	/* XXX return unsent MPAKs here */

	return;
}
