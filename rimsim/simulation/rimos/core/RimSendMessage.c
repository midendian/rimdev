/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSendMessage(TASK hTask, MESSAGE *msg)
{
	printf("sim: RimSendMessage(%ld, %p)\n", hTask, msg);

	return FALSE;
}
