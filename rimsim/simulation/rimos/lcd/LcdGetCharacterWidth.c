/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_LcdGetCharacterWidth(char c, int fontIndex)
{

	SIMTRACE("LcdGetCharacterWidth", "%c, %d", c, fontIndex);

	return 4;
}
