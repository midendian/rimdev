/* -*- Mode: ab-c -*- */

#include <rimsim.h>

TASK sim_RimGetMessage(MESSAGE *msg)
{

	SIMTRACE("RimGetMessage", "%p", msg);

	for (;;) {
		MESSAGE *newmsg;
		TASK poster;

		if ((newmsg = getmessage(&poster))) {
			memcpy(msg, newmsg, sizeof(MESSAGE));
			freemessage(newmsg);
			return poster;
		}

		schedule();
	}

	return 0;
}
