/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RimSprintf(char *Buffer, int Maxlen, char *Fmt, ...)
{
	va_list ap;
	int ret;

	printf("sim: RimSprintf(%p, %d, %s, ...)\n", Buffer, Maxlen, Fmt);
	va_start(ap, Fmt);
	ret = vsnprintf(Buffer, Maxlen, Fmt, ap);
	va_end(ap);

	return ret;
}

