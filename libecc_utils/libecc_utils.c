#include "libecc_utils.h"

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

void my_nn_print(const char* msg, nn_src_t a){
    int ret, w;

	ret = nn_check_initialized(a); EG(ret, err);
	MUST_HAVE(msg != NULL, ret, err);

	kprintf("%s (%d words, i.e. %d bits): 0x", msg, a->wlen,
		   a->wlen * WORD_BYTES * 8);

	for (w = a->wlen - 1; w >= 0; w--) {
		kprintf("%x", a->val[w]);
	}

	kprintf("\n");

err:
	return;
}

void my_ec_point_print(const char *msg, prj_pt_src_t pt){
	aff_pt y_aff;
	int ret, iszero;
	y_aff.magic = WORD(0);

	MUST_HAVE(msg != NULL, ret, err);
	ret = prj_pt_iszero(pt, &iszero); EG(ret, err);
	if (iszero) {
		kprintf("%s: infinity\n", msg);
		goto err;
	}

	ret = prj_pt_to_aff(&y_aff, pt); EG(ret, err);
	kprintf("%s", msg);
	my_nn_print("x", &(y_aff.x.fp_val));
	kprintf("%s", msg);
	my_nn_print("y", &(y_aff.y.fp_val));

err:
	aff_pt_uninit(&y_aff);
	return;
}


int init_curve(const u8* curve_name, ec_params* curve_params){
    int ret;

    /* Importing a specific curve parameters from the constant static
    * buffers describing it:
    * It is possible to import a curves parameters by its name.
    */
    const ec_str_params *the_curve_const_parameters;

    uint32_t len;

    ret = local_strnlen((const char *)curve_name, MAX_CURVE_NAME_LEN, &len); EG(ret, err);
	len += 1;
	MUST_HAVE((len < 256), ret, err);

    kprintf("Curve name: %s\n", curve_name);
	ret = ec_get_curve_params_by_name(curve_name,
					    (u8)len, &the_curve_const_parameters); EG(ret, err);

    /* Get out if getting the parameters went wrong */
	if (the_curve_const_parameters == NULL) {
		// kprintf("Error: error when importing curve %s "
		// 	   "parameters ...\n", curve_name);
            kprintf("Error importing curve data\n");
		ret = -1;
		goto err;
	}


    // nn tmp_order;
    // ret = nn_init_from_buf(&tmp_order,
    // PARAM_BUF_PTR(the_curve_const_parameters->gen_order),
    // PARAM_BUF_LEN(the_curve_const_parameters->gen_order));
    // if(ret != 0){
    // kprintf("Error check order curve inside\n");
    //     goto err;   
    // }else{
    //     kprintf("Curve order OK inside\n");
    // }
    // nn_print("Curve order inside", &tmp_order);


    /* Now map the curve parameters to our libecc internal representation */
	ret = import_params(curve_params, the_curve_const_parameters);
    if(ret != 0){
    kprintf("Error import params\n");
        goto err;   
    }else{
        kprintf("Import params OK\n");
    }

err:
    return ret;
}

int setup_phase(ec_params* curve_params, node* N, server* S){
    int ret;

    //Setup curve for keys
    N->key.priv_key.key_type = ECDSA;
    N->key.priv_key.params = (const ec_params* ) (curve_params);
    N->key.priv_key.magic = PRIV_KEY_MAGIC;
    N->key.pub_key.key_type = ECDSA;
    N->key.pub_key.params = (const ec_params* ) (curve_params);
    N->key.pub_key.magic = PUB_KEY_MAGIC;

    //Setup curve for keys
    S->key.priv_key.key_type = ECDSA;
    S->key.priv_key.params = (const ec_params* ) (curve_params);
    S->key.priv_key.magic = PRIV_KEY_MAGIC;
    S->key.pub_key.key_type = ECDSA;
    S->key.pub_key.params = (const ec_params* ) (curve_params);
    S->key.pub_key.magic = PUB_KEY_MAGIC;

    //Server generates its own long-term keys
    /* Get a random value in [0,q] */
	ret = nn_get_random_mod(&(S->key.priv_key.x), &(curve_params->ec_gen_order));
    if(ret != 0){
        kprintf("-> Server ERROR gen private key\n");
        goto err;
    }
    // prj_pt_src_t G; //Base generator of the curve
    // G = &(curve_params.ec_gen);
    /* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul(&(S->key.pub_key.y), &(S->key.priv_key.x), &(curve_params->ec_gen));
    if(ret != 0){
        kprintf("-> Server ERROR gen public key\n");
        goto err;
    }

err:
    return ret;
}

int cert_request(ec_params* curve_params, node* N){
    int ret;

    //Node generates temporary private number
    /* Get a random value in [0,q] */
	ret = nn_get_random_mod(&(N->ku), &(curve_params->ec_gen_order));
    if(ret != 0){
        kprintf("-> Node ERROR gen temporary private number\n");
        goto err;
    }

    // prj_pt_src_t G; //Base generator of the curve
    // G = &(curve_params.ec_gen);
    /* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul(&(N->Ru), &(N->ku), &(curve_params->ec_gen)); 
    if(ret != 0){
        kprintf("-> Node ERROR gen temporary public point\n");
        goto err;
    }

err:
    return ret;
}

int implicit_cert_gen(ec_params* curve_params, server* S, prj_pt* Ru, unsigned char* ID){
    int ret;

    nn k; //temporary number
    nn e;
    nn ek;
    prj_pt kG;
    prj_pt Pu;
    // Pu.magic = kG.magic = WORD(0);
    nn_init(&k, 0);
    nn_init(&e, 0);
    nn_init(&ek, 0);
    prj_pt_init(&kG, &(curve_params->ec_curve));
    prj_pt_init(&Pu, &(curve_params->ec_curve));

    ret = nn_get_random_mod(&k, &(curve_params->ec_gen_order)); EG(ret, err);
    if(ret != 0){
        kprintf("-> Server ERROR gen temporary private number\n");
        goto err;
    }

    //kG = k*G
    ret = prj_pt_mul(&kG, &k, &(curve_params->ec_gen)); EG(ret, err);
    if(ret != 0){
        kprintf("-> Server ERROR gen temporary public point\n");
        goto err;
    }

    //Pu = Ru + kG
    
    ret = prj_pt_add(&Pu, Ru, &kG);
    if(ret != 0){
        kprintf("-> Server ERROR add point\n");
        goto err;
    }
    ec_point_print("Server Pu ", &Pu);

    // int is_oncurve;
    // aff_pt T;
    // ret = prj_pt_is_on_curve(&Pu, &is_oncurve); EG(ret, err);
	// if (!is_oncurve) {
	// 	ext_printf("Error: C = A+B is not on the %s curve!\n",
	// 		   curve_params.curve_name);
	// 	ret = -1;
	// 	goto err;
	// }

    // ret = prj_pt_to_aff(&T, &Pu); EG(ret, err);
    // ret = aff_pt_is_on_curve(&T, &is_oncurve); EG(ret, err);
	// if (!is_oncurve) {
	// 	ext_printf("Error: Pu = A+B is not on the %s curve!\n",
	// 		   curve_params.curve_name);
	// 	ret = -1;
	// 	goto err;
	// }

    ret = encode_implicit_cert(curve_params, &Pu, ID, S);
    if(ret != 0){
        kprintf("-> Server ERROR encode certificate \n");
        goto err;
    }
    kprintf("\t[Server] Cert: \n");
    my_bio_dump(S->certTemp, CERT_SIZE);

    unsigned char hashed_cert[HASH_SIZE];
    hash_implicit_cert(S->certTemp, hashed_cert);
    kprintf("\t[Server] Hashed cert: \n");
    my_bio_dump(hashed_cert, HASH_SIZE);

    //e = Hash(Cert) mod q
    nn h;
    nn_init(&h, 0);
    my_nn_print("Server h", &h);
    ret = nn_init_from_buf(&h, hashed_cert, HASH_SIZE);
    //mod q
    ret = nn_mod(&e, &h, &(curve_params->ec_gen_order));


    if(ret != 0){
        kprintf("-> Server ERROR convert buf to e number\n");
        goto err;
    }
    my_nn_print("Server h", &h);
    my_nn_print("Server e", &e);

    // r = ek + d_ca (mod q)
    // ret = n_mul_low(&ek, &e, &k, (u8)(e.wlen + k.wlen));
    ret = nn_mul(&ek, &e, &k);
    // ret = nn_mod_add(&(S->r), &ek, &(S->key.priv_key.x), &(curve_params->ec_gen_order));
    ret = nn_add(&ek, &ek, &(S->key.priv_key.x));
    ret = nn_mod_notrim(&(S->r), &ek, &(curve_params->ec_gen_order));

    my_nn_print("Curve order", &(curve_params->ec_gen_order));
    if(ret != 0){
        kprintf("-> Server ERROR add mod for r\n");
        goto err;
    }

    // my_nn_print("Server ek", &ek);
    my_nn_print("Server r", &(S->r));

err:
    return ret;
}

int encode_implicit_cert(ec_params* curve_params, prj_pt* Pu, unsigned char* ID, server* S){
    int ret;
    // uint8_t buf[1000]; //apply fixed value
    // uint16_t coord_len;
    // coord_len= (uint16_t)(3 * BYTECEIL((Pu->crv)->a.ctx->p_bitlen));
    // printf("Coord_len: %d\n",coord_len);
    // if(coord_len > sizeof(buf)){
	// 	ret = -1;
	// 	printf("Error: error when exporting the point\n");
	// 	// goto err;
	// }

    // ret = prj_pt_export_to_buf(Pu, S->certTemp, EXPORTED_POINT_SIZE); //possibly no null character
    // if(ID != NULL){
    //     memcpy((S->certTemp) + EXPORTED_POINT_SIZE, ID, ID_SIZE);
    // }

    // u8 buf[EXPORTED_AFF_POINT_SIZE];
    // printf("Buffer size: %d\n",(u32)(2 * BYTECEIL(curve_params.ec_fp.p_bitlen)));
    ret = prj_pt_export_to_aff_buf(Pu, S->certTemp, EXPORTED_AFF_POINT_SIZE);
    if(ID != NULL){
        memcpy((S->certTemp) + EXPORTED_AFF_POINT_SIZE, ID, ID_SIZE);
    }
    // ret = -1;

err:
    return ret;
}

int hash_implicit_cert(unsigned char* cert, unsigned char* hashed_cert){
    int ret;
    sha256_context ctx;

    ret = sha256_init(&ctx); EG(ret, err);
    ret = sha256_update(&ctx, cert, CERT_SIZE); EG(ret, err);
    ret = sha256_final(&ctx, hashed_cert);

err:
    return ret;
}


int extract_node_key_pair(ec_params* curve_params, node* N, prj_pt* S_pubKey, nn* r, unsigned char* cert){
    int ret;
    
    nn Se;
    // nn Ne;
    nn eku;
    prj_pt Pu;
    prj_pt ePu;
    // Pu.magic = ePu.magic = WORD(0);
    nn_init(&Se, 0);
    // nn_init(&Ne, 0);
    nn_init(&eku, 0);
    prj_pt_init(&Pu, &(curve_params->ec_curve));
    prj_pt_init(&ePu, &(curve_params->ec_curve));


    unsigned char hashed_cert[HASH_SIZE];
    hash_implicit_cert(cert, hashed_cert);
    kprintf("\t[Node] Hashed cert: \n");
    my_bio_dump(hashed_cert, HASH_SIZE);

    

    ret = nn_init_from_buf(&Se, hashed_cert, HASH_SIZE);

    //e = Hash(Cert) mod q
    // nn h;
    // ret = nn_init_from_buf(&h, hashed_cert, HASH_SIZE);
    // //mod q
    // ret = nn_mod(&e, &h, &(curve_params.ec_gen_order));

    
    my_nn_print("\tNode Se", &Se);
    // my_nn_print("Node Ne", &Ne);
    //eku = e * ku
    ret = nn_mul(&eku, &Se, &(N->ku));
    my_nn_print("\tNode eku", &eku);
    // my_nn_print("Curve order", &(curve_params.ec_gen_order));
    //du = eku + r (mod q)
    // ret = nn_mod_add(&(N->key.priv_key.x), &eku, r, &(curve_params->ec_gen_order)); //long-term private key
    ret = nn_add(&eku, &eku, r);
    ret = nn_mod_notrim(&(N->key.priv_key.x), &eku, &(curve_params->ec_gen_order));
    if(ret != 0){
        kprintf("-> Node ERROR calculating private key\n");
        goto err;
    }

    //ePu = e*Pu
    ret = extract_point_from_cert(curve_params, &Pu, cert);
    if(ret != 0){
        kprintf("-> Error extract point\n");
        goto err;
    }
    my_ec_point_print("\tNode Pu ", &Pu);

    ret = prj_pt_mul(&ePu, &Se, &Pu); EG(ret, err);
    if(ret != 0){
        kprintf("-> Node ERROR calculating scalar point mul\n");
        goto err;
    }
    
    //Qu = ePu + Q_CA
    // ec_point_print("Node scalar point mul ePu ", &ePu);
    // ec_point_print("Server public key Q_ca ", S_pubKey);
    // printf("Check inputs success\n");

    ret = prj_pt_add(&(N->key.pub_key.y), &ePu, S_pubKey);
    if(ret != 0){
        kprintf("-> Node ERROR calculating public key\n");
        goto err;
    }

    int result = 0;
    ret = prj_pt_is_on_curve(&(N->key.pub_key.y), &result);
    if(ret != 0){
        kprintf("-> Error calculating check point on curve\n");
        goto err;
    }else{
        if (result != 1){
            kprintf("-> Point not on curve\n");
            ret = -1;
            goto err;
        }else{
            kprintf("-> Extracted point OK\n");
        }
    }
    

err:
    return ret;
}

int extract_point_from_cert(ec_params* curve_params, prj_pt* Pu, unsigned char* cert){
    int ret;

    // ret = prj_pt_import_from_buf(Pu, cert, EXPORTED_POINT_SIZE, &(curve_params.ec_curve));
    // if(ret != 0){
    //     printf("-> Node ERROR extract point from cert\n");
    //     goto err;
    // }
    // kprintf("Check cert in extract:\n");
    // for(int i=0; i<CERT_SIZE;i++){
    //     kprintf("%x",cert[i]);
    // }
    // kprintf("\n");

    ret = prj_pt_import_from_aff_buf(Pu, cert, EXPORTED_AFF_POINT_SIZE, &(curve_params->ec_curve));
    if(ret != 0){
        kprintf("-> Node ERROR extract point from cert\n");
        goto err;
    }

err:
    return ret;
}

int verify_key(ec_params* curve_params, node* N){
    int ret;

    prj_pt Qu_prime;
    // Qu_prime.magic = WORD(0);
    prj_pt_init(&Qu_prime, &(curve_params->ec_curve));

    ret = prj_pt_mul(&Qu_prime, &(N->key.priv_key.x), &(curve_params->ec_gen)); EG(ret, err);

    //verify
    int result = 1;
    ret = prj_pt_cmp(&Qu_prime, &(N->key.pub_key.y), &result);
    if(ret != 0){
        kprintf("Key verifying process ERROR\n");
        goto err;
    }
    if( result != 0){
        kprintf("Key verifying FAILED - Keys can not be used\n");
        my_ec_point_print("Node Qu_prime ", &Qu_prime);
        my_ec_point_print("Node public key Qu ", &(N->key.pub_key.y));
    }else{
        kprintf("Keys verification SUCCESS\n");
    }

err:

    return ret;
}