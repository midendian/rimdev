/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_RadioRegister(void)
{
	MESSAGE msg;

	printf("sim: RadioRegister()\n");

	rim_task_current->flags |= RIM_TASKFLAG_WANTSRADIO;

	if (radio_getstate() == 1) {

		msg.Device = DEVICE_RADIO;
		msg.Event = SIGNAL_LEVEL;
		msg.SubMsg = radio_getrssi();

	} else {

		msg.Device = DEVICE_RADIO;
		msg.Event = RADIO_TURNED_OFF;
		msg.SubMsg = 0;

	}

	msg.Length = 0;
	msg.DataPtr = NULL;
	msg.Data[0] = 0;
	msg.Data[1] = 0;

	sendmessageto(rim_task_current, &msg, RIM_TASK_INVALID);

	return;
}
