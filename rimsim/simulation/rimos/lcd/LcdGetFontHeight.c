/* -*- Mode: ab-c -*- */

#include <rimsim.h>

#define LCD_BAD_FONT_INDEX -3

int sim_LcdGetFontHeight(int fontIndex)
{

	SIMTRACE("LcdGetFontHeight", "%d", fontIndex);

	if ((fontIndex < -1) || (fontIndex > 4))
		return LCD_BAD_FONT_INDEX;

	return 6;
}
