/* -*- Mode: ab-c -*- */

#include <rimsim.h>

HandleType sim_DbAddRec(HandleType db, unsigned size, const void *data)
{
	printf("sim: DbAddRec(%u, %u, %p)\n", db, size, data);

	return 0;
}
