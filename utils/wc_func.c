#include "wc_func.h"

void print_sp(char* str, mp_int* test){
    // int size;
    // mp_radix_size(test, 16,&size);
    // char* temp;
    // temp = (char*)malloc(size);
    // sp_tohex(test, temp);
    // if(str != NULL){
    //     kprintf("%s: ",str);
    // }
    // kprintf("%s\n", temp);
    // my_bio_dump(temp,size);
}

void print_point(ecc_spec* curve, ecc_point* P){
    word32 temp_size = EXPORTED_POINT_SIZE;
    byte temp[EXPORTED_POINT_SIZE];
    wc_ecc_export_point_der_ex(curve->idx, P, temp, &temp_size, 1);
    my_bio_dump(temp, temp_size);
}


void my_bio_dump_line(uint32_t cnt, unsigned char* s, int len){
  kprintf("\r");
  uart_put_hex((void*) uart_reg, cnt*16);
  kprintf(" - ");
  for(int i = 0; i < len; i++){
      uart_put_hex_1b((void*) uart_reg, (uint8_t) s[i]);
  }
  kprintf("\n");
}

void my_bio_dump(unsigned char* s, int len){
  int cnt = len/16;
  for (int line = 0; line <cnt; line++){
      my_bio_dump_line(line, s+line*16, 16);
  }
  int mod = len %(cnt*16);
  if(mod != 0){
      my_bio_dump_line(cnt+1, s+cnt*16,mod);
  }
}


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
                        mp_int* a, mp_int* modulus, mp_int mu, mp_digit mp)
{
    int ret;
    // ecc_point* montP1 = wc_ecc_new_point();
    // ecc_point* montP2 = wc_ecc_new_point();
    ecc_point* montP1 = wc_ecc_new_point_h(HEAP_HINT);
    ecc_point* montP2 = wc_ecc_new_point_h(HEAP_HINT);
 
    //This step check mu == 1, if so, no need to conversion to Montgomery domain
    //Can omit this step since the points are in affine coordinate =>know in advance that a conversion is needed
    if(mp_cmp_d(&mu, 1) == MP_EQ){
        kprintf("Montgomery normalizer is 1\n");
    }else{
        /* Do point conversion here */
        if ((mp_mulmod(P->x, &mu, modulus, montP1->x) != MP_OKAY) ||
                (mp_mulmod(P->y, &mu, modulus, montP1->y) != MP_OKAY) ||
                (mp_mulmod(P->z, &mu, modulus, montP1->z) != MP_OKAY)) {
            kprintf("mp_mulmod error");
            ret = 0;
        }

        if ((mp_mulmod(Q->x, &mu, modulus, montP2->x) != MP_OKAY) ||
                (mp_mulmod(Q->y, &mu, modulus, montP2->y) != MP_OKAY) ||
                (mp_mulmod(Q->z, &mu, modulus, montP2->z) != MP_OKAY)) {
            kprintf("mp_mulmod error");
            ret = 0;
        }
    }

    //R = P + Q
    ret = ecc_projective_add_point(montP1, montP2, R, a, modulus, mp);

    ecc_map(R,modulus,mp);
    // ret = wc_ecc_point_is_on_curve(R, idx);
    // if(ret != MP_OKAY){
    //     kprintf("Add point failed\n");
    //     return ret;
    // }

    return ret;
}

int ecc_custom_add_point(ecc_spec* curve, ecc_point* P, ecc_point* Q, ecc_point* R){
    return _ecc_custom_add_point(P, Q, R, curve->idx, &(curve->af), &(curve->prime), curve->mu, curve->mp);
}


int gen_private_key(ecc_spec* curve, WC_RNG* rng, ecc_key *key){

    int ret;

    ret = wc_ecc_gen_k(rng, KEYSIZE, key->k, &(curve->order));

    return ret;
}

int gen_public_key(ecc_spec* curve, ecc_key *key){
    int ret;
    // ecc_point* pub;
    // pub = &(key->pubkey);
    
    // ret = wc_ecc_mulmod(key->k, curve->G, &(key->pubkey), &(curve->af), &(curve->prime), 1);
    ret = wc_ecc_mulmod_ex(key->k, curve->G, &(key->pubkey), &(curve->af), &(curve->prime), 1,HEAP_HINT);
    return ret;
}

int initial_setup(ecc_spec* curve, server* S, node* N){
    int ret;
    //Getting curve specs
    curve->idx = wc_ecc_get_curve_idx(ECQV_CURVE);
    curve->spec = wc_ecc_get_curve_params(curve->idx);
    kprintf("\t[Server] Curve idx: %d\n", curve->idx);
    kprintf("\t[Server] Curve key size: %d\n", curve->spec->size);
    
    mp_init_multi(&(curve->af),&(curve->prime),&(curve->order),&(curve->mu),NULL,NULL);
    // curve->G = wc_ecc_new_point();
    curve->G = wc_ecc_new_point_h(HEAP_HINT);
    mp_read_radix(&(curve->order), curve->spec->order, 16); //convert const char* to big number base 16
    mp_read_radix(&(curve->af), curve->spec->Af, 16); //convert const char* to big number base 16
    mp_read_radix(&(curve->prime), curve->spec->prime, 16); //convert const char* to big number base 16
    wc_ecc_get_generator(curve->G, curve->idx);

    print_sp("\t[Server] Curve af", &(curve->af));
    print_sp("\t[Server] Curve order", &(curve->order));
    print_sp("\t[Server] Curve prime", &(curve->prime));
    

    /* Calculate the Montgomery normalizer. */
    if (mp_montgomery_calc_normalization(&(curve->mu), &(curve->prime)) != MP_OKAY) {
        kprintf("mp_montgomery_calc_normalization error");
        ret = 0;
    }
    print_sp("\tMontgomery mu", &(curve->mu));

    mp_montgomery_setup(&(curve->prime), &(curve->mp));
    kprintf("\tMontgomery mp: %ld\n", curve->mp);

    //Server gen key pair
    // wc_ecc_init(&(N->key));
    wc_ecc_init_ex(&(N->key), HEAP_HINT, INVALID_DEVID);

    wc_ecc_set_curve(&(N->key), KEYSIZE, ECQV_CURVE);

    // wc_ecc_init(&(S->key));
    wc_ecc_init_ex(&(S->key), HEAP_HINT, INVALID_DEVID);
    wc_ecc_set_curve(&(S->key), KEYSIZE, ECQV_CURVE);
    
    WC_RNG rng;
    // ret = wc_InitRng(rng);
    ret = wc_InitRng_ex(&rng, HEAP_HINT, INVALID_DEVID);
    if(ret != MP_OKAY){
        kprintf("\t-> Server Error init RNG\n");
        kprintf("\rInit RNG failed\n");
        if(ret == DRBG_CONT_FIPS_E){
            kprintf("rng DRBG_CONT_FIPS_E\n");
        }else if(ret == RNG_FAILURE_E){
            kprintf("rng RNG_FAILURE_E\n");
        }else{
            kprintf("rng stop at %d\n", ret);
        }
        return ret;
    }

    ret = gen_private_key(curve, &rng, &(S->key));
    if(ret != MP_OKAY){
        kprintf("-> Server ERROR gen private key\n");
        return ret;
    }

    ret = gen_public_key(curve, &(S->key));
    if(ret != MP_OKAY){
        kprintf("-> Server ERROR gen public key\n");
        return ret;
    }

    ret = wc_ecc_check_key(&(S->key));
    if(ret != MP_OKAY){
        kprintf("-> Server key check FAILED\n");
        return ret;
    }else{
        kprintf("\t[Server] key check SUCCESS\n");
    //     print_sp("\t[Server] d_ca",S->key.k);
    //     kprintf("\t[Server] Q_ca:\n");
    //     print_point(curve, &(S->key.pubkey));
    }

    return ret;
}

int cert_request(ecc_spec* curve, node* N){
    int ret;
    mp_init(&(N->ku));
    // N->Ru = wc_ecc_new_point();
    N->Ru = wc_ecc_new_point_h(HEAP_HINT);
    
    WC_RNG rng;
    // ret = wc_InitRng(&rng);
    ret = wc_InitRng_ex(&rng, HEAP_HINT, INVALID_DEVID);
    if(ret != MP_OKAY){
        kprintf("Error init RNG\n");
        return ret;
    }

    ret = wc_ecc_gen_k(&rng, KEYSIZE, &(N->ku), &(curve->order));

    // ret = wc_ecc_mulmod(&(N->ku), curve->G, N->Ru, &(curve->af), &(curve->prime), 1);
    ret = wc_ecc_mulmod_ex(&(N->ku), curve->G, N->Ru, &(curve->af), &(curve->prime), 1, HEAP_HINT);

    // ret = wc_ecc_point_is_on_curve(N->Ru, curve->idx);
    // if(ret != MP_OKAY){
    //     kprintf("Node random point failed\n");
    //     return ret;
    // }

    return ret;
}

int implicit_cert_gen(ecc_spec* curve, server* S, ecc_point* Ru, byte* NID){
    int ret;
    mp_int k;
    mp_init(&k);
    WC_RNG rng;
    // ret = wc_InitRng(&rng);
    ret = wc_InitRng_ex(&rng, HEAP_HINT, INVALID_DEVID);
    ret = wc_ecc_gen_k(&rng, KEYSIZE, &k, &(curve->order)); //gen private key k

    // ecc_point* kG = wc_ecc_new_point();
    ecc_point* kG = wc_ecc_new_point_h(HEAP_HINT);
    //kG = k*G
    // ret = wc_ecc_mulmod(&k, curve->G, kG, &(curve->af), &(curve->prime), 1); //gen public key kG
    ret = wc_ecc_mulmod_ex(&k, curve->G, kG, &(curve->af), &(curve->prime), 1, HEAP_HINT); //gen public key kG
    // ret = wc_ecc_point_is_on_curve(kG, curve->idx);
    // if(ret != MP_OKAY){
    //     kprintf("Server random point failed\n");
    //     return ret;
    // }

    // ecc_point* Pu = wc_ecc_new_point();
    ecc_point* Pu = wc_ecc_new_point_h(HEAP_HINT);
    ret = ecc_custom_add_point(curve, Ru, kG, Pu);
    if(ret != MP_OKAY){
        kprintf("Error add point\n");
        return ret;
    }

    ret =  encode_implicit_cert(curve, Pu, NID, S->certTemp);
    kprintf("\t[Server] Encoded cert: ");
    for(int i = 0; i < CERT_SIZE; i++){
        kprintf("%x", S->certTemp[i]);
    }
    kprintf("\n");

    byte hashed_cert[HASH_SIZE];
    hash_implicit_cert(S->certTemp, hashed_cert);
    kprintf("\t[Server] Hashed cert: \n");
    my_bio_dump(hashed_cert, HASH_SIZE);
    /* ek = e*k */
    mp_int e;
    mp_int ek;
    mp_init_multi(&e, &ek, NULL,NULL,NULL,NULL);
    ret = mp_read_unsigned_bin(&e, hashed_cert, HASH_SIZE);
    if(ret != MP_OKAY){
        kprintf("Error convert hashed cert to number\n");
        return ret;
    }
    print_sp("\t[Server] e", &e);
    
    mp_mul(&e,&k,&ek);

    /* r = (ek + d_ca) mod n */
    //mp_addmod: Add a to b modulo m into r. r = a + b (mod m)
    mp_init(&(S->r));
    mp_addmod(&ek, S->key.k, &(curve->order),&(S->r));
    print_sp("\t[Server] ek", &ek);
    print_sp("\t[Server] r", &(S->r));
    
    return ret;
}

int encode_implicit_cert(ecc_spec* curve, ecc_point* Pu, byte* NID, byte* cert_buffer){
    int ret;
    word32 size = EXPORTED_POINT_SIZE;
    ret = wc_ecc_export_point_der_ex(curve->idx, Pu, cert_buffer, &size, 1); //insert exported point to cert buffer
    if(ret != MP_OKAY){
        kprintf("Error export point\n");
        return ret;
    }

    if(NID != NULL){
        memcpy(cert_buffer+EXPORTED_POINT_SIZE, NID, ID_SIZE);
    }
    cert_buffer[CERT_SIZE] = '\0';

    return ret;
}


int hash_implicit_cert(byte* data, byte* hashed_cert){
    ECQV_HASH sha256;

    if (wc_InitSha256(&sha256) != 0) {
        kprintf("wc_InitSha256 failed");
    }
    else {
        wc_Sha256Update(&sha256, data, CERT_SIZE-1);/* no need last NULL byte */
        wc_Sha256Final(&sha256, hashed_cert); //result finished here
        wc_Sha256Free(&sha256); //free allocated
    }
    
    // for(int i = 0; i < 32; i++){
    //     kprintf("%x", hashed_cert[i]);
    // }
    // kprintf("\n");

    return 0;
}


int extract_node_key_pair(ecc_spec* curve, node* N, ecc_point* QCA, mp_int* r, byte* certN){
    int ret; 
    mp_int e;
    mp_int eku;

    byte hashed_cert[HASH_SIZE];
    hash_implicit_cert(certN, hashed_cert);
    mp_init_multi(&e, &eku, NULL,NULL,NULL,NULL);
    ret = mp_read_unsigned_bin(&e, hashed_cert, HASH_SIZE);
    if(ret != MP_OKAY){
        kprintf("Error convert hashed cert to number\n");
        return ret;
    }
    
    /*eku = e*ku */
    mp_mul(&e, &(N->ku), &eku);
    
    mp_addmod(&eku, r, &(curve->order), N->key.k); //long-term private key

    // ecc_point* ePu = wc_ecc_new_point();
    ecc_point* ePu = wc_ecc_new_point_h(HEAP_HINT);
    ecc_point* Pu = extract_point_from_cert(curve, certN);
    if(Pu == NULL){
        kprintf("Failed point import\n");
        return -1;
    }

    // ret = wc_ecc_mulmod(&e,Pu,ePu,&(curve->af),&(curve->prime),1);
    ret = wc_ecc_mulmod_ex(&e,Pu,ePu,&(curve->af),&(curve->prime),1, HEAP_HINT);
    ret = ecc_custom_add_point(curve, ePu, QCA, &(N->key.pubkey));

    return ret;
}

ecc_point* extract_point_from_cert(ecc_spec* curve, byte* cert){
    int ret;
    // ecc_point* Pu = wc_ecc_new_point();
    ecc_point* Pu = wc_ecc_new_point_h(HEAP_HINT);
    ret = wc_ecc_import_point_der_ex(cert, EXPORTED_POINT_SIZE, curve->idx, Pu, 0);
    if(ret != MP_OKAY){
        kprintf("Error import point from cert\n");
        return NULL;
    }

    return Pu;
}
