#ifndef _DRIVER_XPR_H
#define _DRIVER_XPR_H


#ifndef __ASSEMBLER__

#define MAX_WAIT_TIME 1000000
#define XPR_ERROR_WAIT -1
#define XPR_ERROR_RANDOM -2
#define XPR_OK 0

#include <stdint.h>

void xpr_reset(void* xpr_reg);
void xpr_reset_and_disable(void* xpr_reg);

int xpr_setup(void* xpr_reg, uint32_t delay, uint32_t pair_selection);
uint32_t xpr_get_random(void* xpr_reg);

int xpr_xor_puf(void* xpr_reg, uint32_t delay, uint32_t pair_selection);
int xpr_xor_puf_trigger1(void* xpr_reg, uint32_t pair_selection);
int xpr_xor_puf_trigger2(void* xpr_reg, uint32_t pair_selection);

int xpr_puf_mode(void * xpr_reg, uint32_t delay, uint32_t mask, uint32_t pair_selection, uint32_t challenge);



#endif /* !__ASSEMBLER__ */

#endif /* _DRIVERS_TRNG_H */
