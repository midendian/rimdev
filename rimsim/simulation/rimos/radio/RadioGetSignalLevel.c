/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RadioGetSignalLevel(void)
{

	SIMTRACE_NOARG("RadioGetSignalLevel");

	return radio_getrssi();
}
