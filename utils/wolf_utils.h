#ifndef WOLF_UTIL_H
#define WOLF_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include <wolfssl/options.h>
//#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/sha256.h>
//#include <wolfssl/openssl/ssl.h>
#include <wolfssl/openssl/ec.h>
#include <wolfssl/openssl/bio.h>
#include <wolfssl/openssl/bn.h>


#define ECQV_EC_CURVE NID_secp256k1
#define ECQV_HASH wc_Sha256
#define DEBUG

typedef struct user{
    EC_KEY* key;
    WOLFSSL_BIGNUM* ku;
    EC_POINT* Ru;
    byte* UID;
} user;

typedef struct server{
    EC_KEY* key;
    WOLFSSL_BIGNUM* r;
    byte* CertU;
    word64 CertU_len;
} server;

/* ECQV */
// unsigned char* print_b64_stream(const unsigned char* msg, size_t len);
// unsigned char* ecqv_bn2b64(const WOLFSSL_BIGNUM* bn);
// void ecqv_bn_print(const WOLFSSL_BIGNUM* bn);


#endif //WOLF_UTIL_H