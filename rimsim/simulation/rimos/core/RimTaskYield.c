/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimTaskYield(void)
{
	printf("sim: RimTaskYield()\n");

	/* Let exactly one app run and then come back. (??) */
	schedule();

	return;
}
