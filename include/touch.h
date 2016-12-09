#ifndef TOUCH_included
#define TOUCH_included

#include <gfx-types.h>

extern void touch_init(void);

// How many fingers are touching?  0, 1 or 2.
extern size_t touch_count(void);

// Get the coordinates of the index'th touch.
// 0 <= index < 2.
extern gfx_ipoint touch_point(size_t index);

#endif /* !TOUCH_included */
