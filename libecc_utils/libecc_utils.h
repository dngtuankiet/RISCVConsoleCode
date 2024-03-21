#ifndef __LIBECC_UTILS__
#define __LIBECC_UTILS__
#pragma once


// #include <stdio.h>
// #include <stdlib.h>
#include <string.h>
// #include <stdint.h>

#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>
#include <libecc/libarith.h>
#include <libecc/libec.h>
#include <libecc/libsig.h>
#include <libecc/nn/nn_div.h>

/* We include the printf external dependency for printf output */
#include <libecc/external_deps/print.h>
/* We include the time external dependency for performance measurement */
#include <libecc/external_deps/time.h>
#include <libecc/external_deps/rand.h>
// #include <libecc/sig/ec_key.h>
// #include <libecc/nn/nn_mul.h>


//include from RISCVConsole
#include <kprintf/kprintf.h>
#include <uart/uart.h>
#include "main.h"
#include "encoding.h"

#define CURVE_NAME "SECP256K1"

#define EXPORTED_POINT_SIZE 96
#define EXPORTED_AFF_POINT_SIZE 64
#define HASH_SIZE 32
#define KEYSIZE 32
#define ID_SIZE 8
#define CERT_SIZE (EXPORTED_AFF_POINT_SIZE + ID_SIZE)

#ifdef CUSTOM_RAND_GENERATE_BLOCK
    /* Seed Source */
    /* Size of returned HW RNG value */
    #define CUSTOM_RAND_TYPE      unsigned int
    extern int my_rng_gen_block(unsigned char* buf, u16 len);
    #undef  CUSTOM_RAND_GENERATE_BLOCK
    #define CUSTOM_RAND_GENERATE_BLOCK  my_rng_gen_block

#endif

/* libecc internal structure holding the curve parameters */
static  uint8_t curve_name[MAX_CURVE_NAME_LEN] = CURVE_NAME;
static ec_params curve_params;

typedef struct node {
    ec_key_pair key; //node long-term keys
    nn ku; //random material
    prj_pt Ru; //point material
    unsigned char id[ID_SIZE];
} node;

typedef struct server{
    ec_key_pair key; //server long-term keys
    // byte* certN;
    // unsigned char certN[NODE_COUNT][CERT_SIZE];
    unsigned char certTemp[CERT_SIZE]; //certificate
    nn r; //big number
    //implicit_cert = {r,certificate}
} server;


void my_bio_dump(unsigned char* s, int len);
void my_bio_dump_line(uint32_t cnt, unsigned char* s, int len);
void my_nn_print(const char* msg, nn_src_t a);
void my_ec_point_print(const char *msg, prj_pt_src_t pt);

int init_curve(const u8* curve_name, ec_params* curve_params);
int setup_phase(ec_params* curve_params, node* N, server* S);
int cert_request(ec_params* curve_params, node* N);
int implicit_cert_gen(ec_params* curve_params, server* S, prj_pt* Ru, unsigned char* ID);
int encode_implicit_cert(ec_params* curve_params, prj_pt* Pu, unsigned char* ID, server* S);
int hash_implicit_cert(unsigned char* cert, unsigned char* hashed_cert);

int extract_node_key_pair(ec_params* curve_params, node* N, prj_pt* S_pubKey, nn* r, unsigned char* cert);
int extract_point_from_cert(ec_params* curve_params, prj_pt* Pu, unsigned char* cert);

int verify_key(ec_params* curve_params, node* N);


//basic
int kiet_nn_get_random_mod(nn_t out, nn_src_t q);

int kiet_prj_pt_mul(prj_pt_t out, nn_src_t m, prj_pt_src_t in);
static int kiet_prj_pt_mul_ltr_monty_ladder(prj_pt_t out, nn_src_t m, prj_pt_src_t in);
static int kiet_blind_projective_point(prj_pt_t out, prj_pt_src_t in);

int kiet_prj_pt_add(prj_pt_t out, prj_pt_src_t in1, prj_pt_src_t in2);
static int kiet__prj_pt_add_monty_cf(prj_pt_t out, prj_pt_src_t in1, prj_pt_src_t in2);

#endif //__LIBECC_UTILS__

