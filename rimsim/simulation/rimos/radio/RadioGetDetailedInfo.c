/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RadioGetDetailedInfo(RADIO_INFO *info)
{

	printf("sim: RadioGetDetailedInfo(%p)\n", info);

	if (!info)
		return;

	info->LocalMAN = radio_getman();
	info->ESN = radio_getesn();
	info->Base = info->Area = 42;
	info->RSSI = radio_getrssi();
	info->NetworkID = 0xb433;
	info->FaultBits = 0;
	info->Active = 0; /* "?" -- Official Documentation */
	info->PowerSaveMode = 1; /* "It is always 1 on the handheld." */
	info->LiveState = 1;
	info->TransmitterEnabled = 1;

	return;
}
