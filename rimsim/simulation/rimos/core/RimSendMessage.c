/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSendMessage(TASK hTask, MESSAGE *msg)
{

	SIMTRACE("RimSendMessage", "%ld, %p", hTask, msg);

	return FALSE;
}
