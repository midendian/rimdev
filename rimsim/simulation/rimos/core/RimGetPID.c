/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimGetPID(TASK hTask, PID *Pid, const char **Subtitle)
{
	printf("sim: RimGetPID(%ld, %p, %p)\n", hTask, Pid, Subtitle);

	return FALSE;
}
