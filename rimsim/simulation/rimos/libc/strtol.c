/* -*- Mode: ab-c -*- */

#include <rimsim.h>

long sim_strtol(const char *nptr, char **endptr, int base)
{
	printf("sim: strtol(%s, %p, %d)\n", nptr, endptr, base);

	return 0;
}
