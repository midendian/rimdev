/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimDebugPrintf(const char *Format, ...)
{
	va_list ap;

	printf("sim: RimDebugPrintf(%s)\n", Format);
	va_start(ap, Format);
	vfprintf(stderr, Format, ap);
	va_end(ap);

	return;
}
