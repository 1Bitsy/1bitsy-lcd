#ifndef INTR_included
#define INTR_included

#include <libopencm3/cm3/cortex.h>

#define WITH_INTERRUPTS_MASKED                                          \
     for (bool wim_interrupts_are_masked = cm_is_masked_interrupts(),   \
               wim_first_time = (cm_disable_interrupts(), true);        \
          wim_first_time;                                               \
          wim_interrupts_are_masked ? (void)0 : cm_enable_interrupts(), \
          wim_first_time = false)

#endif /* !INTR_included */
