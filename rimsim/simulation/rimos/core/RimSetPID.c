/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimSetPID(PID *pid)
{
	printf("sim: RimSetPID({%s, %d, %p})\n", pid->Name, pid->EnableForeground, pid->Icon);

	return;
}
