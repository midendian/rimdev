/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdRasterOp(DWORD wOp, DWORD wWide, DWORD wHigh, const BitMap *src, int SrcX, int SrcY, BitMap *dest, int DestX, int DestY)
{
	printf("sim: LcdRasterOp(%ld, %ld, %ld, %p, %d, %d, %p, %d, %d)\n", wOp, wWide, wHigh, src, SrcX, SrcY, dest, DestX, DestY);

	return;
}
