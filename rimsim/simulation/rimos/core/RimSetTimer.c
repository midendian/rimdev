/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSetTimer(DWORD timerID, DWORD time, DWORD type)
{
	printf("sim: RimSetTimer(%ld, %ld, %ld)\n", timerID, time, type);

	return FALSE;
}
