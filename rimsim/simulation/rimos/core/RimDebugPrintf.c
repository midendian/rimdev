/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimDebugPrintf(const char *Format, ...)
{
	va_list ap;

	SIMTRACE("RimDebugPrintf", "%s", Format);

	va_start(ap, Format);
	debugprintf(Format, ap);
	va_end(ap);

	return;
}
