/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimWaitForSpecificMessage(MESSAGE *msg, MESSAGE *compare, DWORD mask)
{
	printf("sim: RimWaitForSpecificMessage(%p, %p, 0x%08lx)\n", msg, compare, mask);

	return FALSE;
}
