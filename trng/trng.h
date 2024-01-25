#ifndef _DRIVERS_TRNG_H
#define _DRIVERS_TRNG_H


#ifndef __ASSEMBLER__

#define MAX_WAIT_TIME 1000000
#define TRNG_ERROR_WAIT -1
#define TRNG_ERROR_RANDOM -2
#define TRNG_OKAY 0

#include <stdint.h>
#include "user_settings.h"

#include "main.h"
#include <platform.h>
#include <string.h>

#ifdef CUSTOM_LIBECC
    #include "libecc_utils/libecc_utils.h"
#endif //CUSTOM_LIBECC

void trng_reset(void* trngctrl);
void trng_reset_disable(void* trngctrl);

int trng_setup(void* trngctrl, uint32_t delay);
uint32_t trng_get_random(void* trngctrl);



#endif /* !__ASSEMBLER__ */

#endif /* _DRIVERS_TRNG_H */
