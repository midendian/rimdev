/* -*- Mode: ab-c -*- */

#include <rimsim.h>

/*
 * RimGetTicks
 *
 * Return the real time elapsed since bootup, in units of 
 * centiseconds (10 milliseconds).
 */
long sim_RimGetTicks(void)
{

	printf("sim: RimGetTicks()\n");

	return timer_getticks();
}
