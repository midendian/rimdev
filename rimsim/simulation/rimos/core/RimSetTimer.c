/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSetTimer(DWORD timerID, DWORD time, DWORD type)
{

	SIMTRACE("RimSetTimer", "%ld, %ld, %ld", timerID, time, type);

	return (timer_set(rim_task_current, timerID, type, time) == -1) ? FALSE : TRUE;
}
