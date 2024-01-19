#ifdef WOLFSSL_USER_SETTINGS
    #include "user_settings.h"
#else
    #include <wolfssl/options.h>
    #include <wolfssl/wolfcrypt/settings.h>
#endif


#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/sp_int.h>
#include <wolfssl/wolfcrypt/integer.h>
#include <wolfssl/wolfcrypt/wolfmath.h>
#include <wolfssl/wolfcrypt/memory.h>

#include "uart/uart.h"
#include "trng/trng.h"
#include <kprintf/kprintf.h>
#include "main.h"


#ifdef WOLFSSL_STATIC_MEMORY
    #define STATIC_MEM_SIZE (2000*1024)
    static WOLFSSL_HEAP_HINT* HEAP_HINT;
    static byte gMemory[STATIC_MEM_SIZE];
#endif

#define ECQV_CURVE ECC_SECP256K1
#define ECQV_HASH wc_Sha256

#define HASH_SIZE 32
#define KEYSIZE 32
#define ID_SIZE 8
#define EXPORTED_POINT_SIZE 33 //including null byte
#define CERT_SIZE (EXPORTED_POINT_SIZE+ID_SIZE)
#define NODE_COUNT 1

typedef struct node{
    ecc_key key;
    mp_int ku;
    ecc_point* Ru;
    byte id[ID_SIZE];
    // byte* id;
} node;

typedef struct server{
    ecc_key key;
    // byte* certN;
    byte certN[NODE_COUNT][CERT_SIZE];
    byte certTemp[CERT_SIZE]; //certificate
    mp_int r; //big number
    //implicit_cert = {r,certificate}
} server;

typedef struct ecc_spec{
    const ecc_set_type* spec;
    mp_int prime;
    mp_int af;
    mp_int order;
    ecc_point* G;
    int idx;
    /* Montgomery parameters */
    mp_int mu; // Montgomery normalizer
    mp_digit mp; // The "b" value from montgomery_setup()
} ecc_spec;

void print_sp(char* str, mp_int* test);
void print_point(ecc_spec* curve, ecc_point* P);
// void my_bio_dump_line(int cnt, const unsigned char* s, int len);
// void my_bio_dump(const unsigned char* s, int len);

void my_bio_dump(unsigned char* s, int len);
void my_bio_dump_line(uint32_t cnt, unsigned char* s, int len);


/**
 * Add two ECC points using Montgomery
 * This function converts all points into Montgomery domain
 * And return the destination point in affine coordinate
 * P        The point to add
 * Q        The point to add
 * R        [out] The destination of the double
 * idx      ECC curve idx
 * a        ECC curve parameter a
 * modulus  The modulus of the filed the ECC curve is in
 * mu       Montgomery normalizer
 * mp       The "b" value from montgomery_setup()
 * return MP_OKAY on success
*/
int _ecc_custom_add_point(ecc_point* P, ecc_point* Q, ecc_point* R, int idx,
                             mp_int* a, mp_int* modulus, mp_int mu, mp_digit mp);

int ecc_custom_add_point(ecc_spec* curve, ecc_point* P, ecc_point* Q, ecc_point* R);

// int gen_private_key(WC_RNG* rng, ecc_key *key, int keySize, int curveID);
// int gen_public_key(ecc_key *key, int curveID);
// int initial_setup(server* S, node* N);

int gen_private_key(ecc_spec* curve, WC_RNG* rng, ecc_key *key);
int gen_public_key(ecc_spec* curve, ecc_key *key);
int initial_setup(ecc_spec* curve, server* S, node* N);

int cert_request(ecc_spec* curve, node* N);
int implicit_cert_gen(ecc_spec* curve, server* S, ecc_point* Ru, byte* NID);

/**
 * Encode certificate to be issued to NID
*/
int encode_implicit_cert(ecc_spec* curve, ecc_point* Pu, byte* NID, byte* cert_buffer);
int hash_implicit_cert(byte* data, byte* hashed_cert);

int extract_node_key_pair(ecc_spec* curve, node* N, ecc_point* QCA, mp_int* r, byte* certN);
ecc_point* extract_point_from_cert(ecc_spec* curve, byte* cert);


