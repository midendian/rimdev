/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_LcdPutStringXY(int x, int y, const char *s, int length, int flags)
{

	printf("sim: LcdPutStringXY(%d, %d, %s, %d, %d)\n", x, y, s, length, flags);

	return 0;
}
