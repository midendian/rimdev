/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimPostMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimPostMessage(%ld, %p)\n", hTask, msg);

	return;
}
