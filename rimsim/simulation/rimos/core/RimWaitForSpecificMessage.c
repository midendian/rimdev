/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimWaitForSpecificMessage(MESSAGE *msg, MESSAGE *compare, DWORD mask)
{

	SIMTRACE("RimWaitForSpecificMessage", "%p, %p, 0x%08lx", msg, compare, mask);

	return FALSE;
}
