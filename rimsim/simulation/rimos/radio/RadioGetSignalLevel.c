/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RadioGetSignalLevel(void)
{

	printf("sim: RadioGetSignalLevel()\n");

	return radio_getrssi();
}
