/* -*- Mode: ab-c -*- */

#include <rimsim.h>

TASK sim_RimGetMessage(MESSAGE *msg)
{
	printf("sim: RimGetMessage(%p)\n", msg);

	fflush(stdout);

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
