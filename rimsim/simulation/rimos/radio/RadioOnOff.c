/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_RadioOnOff(int mode)
{
	int oldstate;

	printf("sim: RadioOnOff(%d)\n", mode);

	oldstate = radio_getstate() ? RADIO_ON : RADIO_OFF;

	if (mode == RADIO_ON)
		radio_setstate(1);
	else if (mode == RADIO_OFF)
		radio_setstate(0);

	return oldstate;
}
