/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSetTimer(DWORD timerID, DWORD time, DWORD type)
{

	printf("sim: RimSetTimer(%ld, %ld, %ld)\n", timerID, time, type);

	return (timer_set(rim_task_current, timerID, type, time) == -1) ? FALSE : TRUE;
}
