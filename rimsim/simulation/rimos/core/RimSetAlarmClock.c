/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimSetAlarmClock(TIME *time, BOOL enable)
{
	printf("sim: RimSetAlarmClock(%p, %d)\n", time, enable);

	return FALSE;
}
