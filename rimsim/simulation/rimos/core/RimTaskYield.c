/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimTaskYield(void)
{

	SIMTRACE_NOARG("RimTaskYield");

	/* Let exactly one app run and then come back. (??) */
	schedule();

	return;
}
