/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void *sim_RimRealloc(void *Ptr, DWORD size)
{

	SIMTRACE("RimRealloc", "%p, %ld", Ptr, size);

	return realloc(Ptr, size);
}
