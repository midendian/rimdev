/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimSetPID(PID *pid)
{

	SIMTRACE("RimSetPID", "{%s, %d, %p}", pid->Name, pid->EnableForeground, pid->Icon);

	return;
}
