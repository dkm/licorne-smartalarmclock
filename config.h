#pragma once

/* When using debug over UART */
static const unsigned long serial_speed = 115200UL;

/* For things that could have real impact on execution */
#ifndef DEBUG
#define DEBUG (1)
#endif

/* For things that may have an impact, but should still be okay */
#ifndef MODERATE_DEBUG
#define MODERATE_DEBUG (0)
#endif

/* For very light debug that has *no* chance to interfere */
/* significatively with execution */
#ifndef WEAK_DEBUG
#define WEAK_DEBUG (1)
#endif
