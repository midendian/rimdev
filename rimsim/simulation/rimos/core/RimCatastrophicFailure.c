/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimCatastrophicFailure(char *FailureMessage)
{
	printf("sim: RimCatastrophicFailure(%s)\n", FailureMessage);

	abort();

	return;
}
