/* -*- Mode: ab-c -*- */

#include <rimsim.h>

TASK sim_RimGetMessage(MESSAGE *msg)
{
	printf("sim: RimGetMessage(%p)\n", msg);

	fflush(stdout);
	for(;;)
		;

	return 0;
}