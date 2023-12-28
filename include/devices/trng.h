
#ifndef _HANKEN_TRNG_H
#define _HANKEN_TRNG_H

#define TRNG_CONTROL  (0x00)
#define TRNG_STATUS   (0x04)
#define TRNG_DELAY    (0x08)
#define TRNG_RANDOM   (0x0C)

#define TRNG_RESET (0x1<<8)
#define TRNG_ENABLE (0x1<<0)
#define TRNG_NEXT (0x1<<1)

#define TRNG_DISABLE_MODE (0x0)
#define TRNG_ENABLE_MODE (((~TRNG_RESET) & (~TRNG_NEXT)) | TRNG_ENABLE)
#define TRNG_RESET_AND_DISABLE_MODE TRNG_RESET

#define TRNG_VALID_BIT (0x1<<0)

#endif /* _HANKEN_TRNG_H */