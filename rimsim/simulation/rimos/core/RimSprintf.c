/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RimSprintf(char *Buffer, int Maxlen, char *Fmt, ...)
{
	va_list ap;
	int ret;

	SIMTRACE("RimSprintf", "%p, %d, %s", Buffer, Maxlen, Fmt);
	va_start(ap, Fmt);
	ret = vsnprintf(Buffer, Maxlen, Fmt, ap);
	va_end(ap);

	return ret;
}

