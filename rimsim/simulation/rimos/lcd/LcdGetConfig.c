/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdGetConfig(LcdConfig *Config)
{

	printf("sim: LcdGetConfig(%p)\n", Config);

	Config->LcdType = 0;
	Config->contrastRange = 100;
	Config->width = 60;
	Config->height = 120;

	return;
}
