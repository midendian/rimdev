/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdScroll(int pixels)
{

	SIMTRACE("LcdScroll", "%d", pixels);

	gui_scroll(pixels);

	return;
}
