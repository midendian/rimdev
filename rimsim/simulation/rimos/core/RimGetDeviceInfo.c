/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RimGetDeviceInfo(DEVICE_INFO *Info)
{

	SIMTRACE("RimGetDeviceInfo", "%p", Info);

	if (!Info)
		return;

	memset(Info, 0, sizeof(DEVICE_INFO));

	Info->deviceType = DEV_HANDHELD;
	Info->networkType = NET_MOBITEX;
	Info->reserved1 = 0;
	Info->deviceId.MAN = radio_getman();
	Info->ESN = radio_getesn();

	return;
}
