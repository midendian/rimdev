/* -*- Mode: ab-c -*- */

#include <rimsim.h>

void sim_LcdGetConfig(LcdConfig *Config)
{

	SIMTRACE("LcdGetConfig", "%p", Config);

	Config->LcdType = 0;
	Config->contrastRange = 31;

	/*
	 * For a Pager, its 65x132; for a handheld its 160x160.
	 */
	Config->width = 65;
	Config->height = 132;

	return;
}
