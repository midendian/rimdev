/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void *sim_memmove(void *dest, const void *src, size_t len)
{
	printf("sim: memmove(%p, %p, %d)\n", dest, src, len);

	return NULL;
}
