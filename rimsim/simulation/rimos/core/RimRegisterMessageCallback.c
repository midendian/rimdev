/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimRegisterMessageCallback(DWORD MessageBits, DWORD MaskBits, CALLBACK_FUNC HandlerFunc)
{

	SIMTRACE("RimRegisterMessageCallback", "0x%08lx, 0x%08lx, %p", MessageBits, MaskBits, HandlerFunc);

	return FALSE;
}
