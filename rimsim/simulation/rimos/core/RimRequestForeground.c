/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimRequestForeground(TASK hTask)
{
	printf("sim: RimRequestForeground(%ld)\n", hTask);

	return TRUE;
}
