/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void *sim_RimMalloc(DWORD size)
{

	SIMTRACE("RimMalloc", "%ld", size);

	return malloc(size);
}
