/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimReplyMessage(TASK hTask, MESSAGE *msg)
{

	SIMTRACE("RimReplyMessage", "%ld, %p", hTask, msg);

	return FALSE;
}
