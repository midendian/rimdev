/* -*- Mode: ab-c -*- */

#include <rimsim.h>

STATUS sim_DbAndRec(HandleType rec, void *mask, unsigned size, unsigned offset)
{
	printf("sim: DbAndRec(%u, %p, %u, %u)\n", rec, mask, size, offset);

	return 0;
}
