/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSendSyncMessage(TASK hTask, MESSAGE *msg, MESSAGE *replyMsg)
{
	printf("sim: RimSendSyncMessage(%ld, %p, %p)\n", hTask, msg, replyMsg);

	return FALSE;
}
