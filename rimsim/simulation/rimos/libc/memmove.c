/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void *sim_memmove(void *dest, const void *src, size_t len)
{

	SIMTRACE("memmove", "%p, %p, %d", dest, src, len);

	return memmove(dest, src, len);
}
