/* -*- Mode: ab-c -*- */

#include <rimsim.h>

TASK sim_RimCreateThread(void (*entry)(void), DWORD stacksize)
{
	printf("sim: RimCreateThread(%p, %ld)\n", entry, stacksize);

	return 0;
}
