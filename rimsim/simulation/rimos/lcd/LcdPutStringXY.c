/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_LcdPutStringXY(int x, int y, const char *s, int length, int flags)
{
	int reallen;

	SIMTRACE("LcdPutStringXY", "%d, %d, %s, %d, %d", x, y, s, length, flags);

	reallen = (length == -1) ? strlen(s) : length;

	/* XXX handle flags */
	gui_putstring(x, y, s, reallen);

	return 0;
}
