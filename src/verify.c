/*
 * Last Modified: Oct 17, 1995
 */

/*
 * exiting functions
 */

/*
 * verify.c - exiting functions
 *
 * UltraRogue: Further Adventures in the Dungeons of Doom 
 *
 * Based on  "Rogue: Exploring the dungeons of doom" Copyright (C) 1980,1981 by
 * Michael Toy, Ken Arnold and Glenn Wichman All rights reserved 
 *
 * Portions Copyright (C) 1984,1991 by Herb Chong 
 *
 * THIS WORK CONTAINS UNPUBLISHED PROPRIETARY SOURCE CODE OF HERB CHONG The
 * copyright notice above does not evidence any actual or intended
 * publication of such source code. 
 *
 * Possession, redistribution, or storage of this source code in any form, in an
 * electronic retrieval system or otherwise, without prior written consent of
 * Herb Chong is strictly prohibited. 
 *
 * See the file LICENSE.TXT for more information. 
 *
 * History
 *
 *  10/17/95 UltraRogue 2.10, Herb Chong
 */

static char sccsid[] = "%W% %G%";

#include "rogue.h"

void verify_function(const char *file, const int line)
{
	char s[80];

	sprintf(s, "Verify failure in %s at line %d\n", file, line);
	fatal(s);
}
