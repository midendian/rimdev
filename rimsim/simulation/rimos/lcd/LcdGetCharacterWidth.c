/* -*- Mode: ab-c -*- */

#include <rimsim.h>

int sim_LcdGetCharacterWidth(char c, int fontIndex)
{
	printf("sim: LcdGetCharacterWidth(%c, %d)\n", c, fontIndex);

	return 4;
}
