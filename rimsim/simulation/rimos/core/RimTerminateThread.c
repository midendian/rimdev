/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimTerminateThread(void)
{

	SIMTRACE_NOARG("RimTerminateThread");

	/* XXX whoa */
	for ( ; ; )
		schedule();

	return;
}
