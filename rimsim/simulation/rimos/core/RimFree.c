/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimFree(void *block)
{

	SIMTRACE("RimFree", "%p", block);

	free(block);

	return;
}
