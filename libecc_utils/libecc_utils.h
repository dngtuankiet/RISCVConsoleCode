#ifndef __LIBECC_UTILS__
#define __LIBECC_UTILS__

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>
#include <libecc/libarith.h>
#include <libecc/libec.h>
#include <libecc/libsig.h>

/* We include the printf external dependency for printf output */
#include <libecc/external_deps/print.h>
/* We include the time external dependency for performance measurement */
#include <libecc/external_deps/time.h>
#include <libecc/external_deps/rand.h>
// #include <libecc/sig/ec_key.h>
// #include <libecc/nn/nn_mul.h>

#define CURVE_NAME "SECP256K1"

#define EXPORTED_POINT_SIZE 96
#define EXPORTED_AFF_POINT_SIZE 64
#define HASH_SIZE 32
#define KEYSIZE 32
#define ID_SIZE 8
#define CERT_SIZE (EXPORTED_AFF_POINT_SIZE + ID_SIZE)


/* Seed Source */
/* Size of returned HW RNG value */
#define CUSTOM_RAND_TYPE      unsigned int
extern unsigned int my_rng_seed_gen(void);
#undef  CUSTOM_RAND_GENERATE
#define CUSTOM_RAND_GENERATE  my_rng_seed_gen

#endif //__LIBECC_UTILS__

