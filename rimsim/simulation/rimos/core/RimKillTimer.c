/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimKillTimer(DWORD timerID)
{
	printf("sim: RimKillTimer(%ld)\n", timerID);

	timer_kill(rim_task_current, timerID);

	return;
}
