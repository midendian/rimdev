/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimKillTimer(DWORD timerID)
{

	SIMTRACE("RimKillTimer", "%ld", timerID);

	timer_kill(rim_task_current, timerID);

	return;
}
