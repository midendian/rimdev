/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdRasterOp(DWORD wOp, DWORD wWide, DWORD wHigh, const BitMap *src, int SrcX, int SrcY, BitMap *dest, int DestX, int DestY)
{

	SIMTRACE("LcdRasterOp", "%ld, %ld, %ld, %p, %d, %d, %p, %d, %d", wOp, wWide, wHigh, src, SrcX, SrcY, dest, DestX, DestY);

	if (wOp == COPY_SRC)
		gui_putbitmap(src->data, src->wide, src->high, DestX, DestY);

	return;
}
