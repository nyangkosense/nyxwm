#include "nyxwm.h"

static const Block blocks[] = {
    /*Icon*/    /*Command*/                                                    /*Update Interval*/    /*Update Signal*/
    {"", "date '+%b %d (%a) %I:%M%p'",                                         5,                     0},  // Changed %p to %P
    {"", "curl -s 'wttr.in/?format=%C+%t'",                            3600,                  0},  // Update every hour
};

static char delim[] = "  ";
static unsigned int delimLen = 5;