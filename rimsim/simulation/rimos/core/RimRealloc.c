/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void *sim_RimRealloc(void *Ptr, DWORD size)
{
	printf("sim: RimRealloc(%p, %ld)\n", Ptr, size);

	return NULL;
}
