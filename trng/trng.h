#ifndef _DRIVERS_TRNG_H
#define _DRIVERS_TRNG_H


#ifndef __ASSEMBLER__

#define MAX_WAIT_TIME 1000000
#define TRNG_ERROR_WAIT -1
#define TRNG_ERROR_RANDOM 0

#include <stdint.h>

void trng_reset(void* trngctrl);
void trng_reset_disable(void* trngctrl);

int trng_setup(void* trngctrl, uint32_t delay);
uint32_t trng_get_random(void* trngctrl);



#endif /* !__ASSEMBLER__ */

#endif /* _DRIVERS_TRNG_H */
