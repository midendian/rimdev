/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSendSyncMessage(TASK hTask, MESSAGE *msg, MESSAGE *replyMsg)
{

	SIMTRACE("RimSendSyncMessage", "%ld, %p, %p", hTask, msg, replyMsg);

	return FALSE;
}
