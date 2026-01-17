/* blocks.h */
#ifndef BLOCKS_H
#define BLOCKS_H

#include "nyxwm.h"

static const Block blocks[] = {
	/* Icon */	/* Command */						/* Interval */	/* Signal */
	{ "", "date '+%b %d (%a) %I:%M%p'", 5, 0 },
	{ "", "curl -s 'wttr.in/?format=%C+%t'", 3600, 0 },
};

static char delim[] = "  ";
static unsigned int delimLen = 5;

#endif /* BLOCKS_H */
