/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimReplyMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimReplyMessage(%ld, %p)\n", hTask, msg);

	return FALSE;
}
