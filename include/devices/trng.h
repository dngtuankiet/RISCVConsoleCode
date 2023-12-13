
#ifndef _HANKEN_TRNG_H
#define _HANKEN_TRNG_H

#define TRNG_CONTROL  (0x00)
#define TRNG_STATUS   (0x04)
#define TRNG_DELAY    (0x08)
#define TRNG_RANDOM   (0x0C)

#define TRNG_RESET (0x1<<8)
#define TRNG_ENABLE (0x1<<0)
#define TRNG_VALID_BIT (0x1<<8)
#define TRNG_READY_BIT (0x1<<0)

#endif /* _HANKEN_TRNG_H */