/* -*- Mode: ab-c -*- */

#include <rimsim.h>

BOOL sim_RimRegisterMessageCallback(DWORD MessageBits, DWORD MaskBits, CALLBACK_FUNC HandlerFunc)
{
	printf("sim: RimRegisterMessageCallback(0x%08lx, 0x%08lx, %p)\n", MessageBits, MaskBits, HandlerFunc);

	return FALSE;
}
