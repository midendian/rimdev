/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdDrawLine(int iDrawingMode, int x1, int y1, int x2, int y2)
{

	SIMTRACE("LcdDrawLine", "%d, %d, %d, %d, %d", iDrawingMode, x1, y1, x2, y2);

	gui_drawline(iDrawingMode, x1, y1, x2, y2);

	return;
}
