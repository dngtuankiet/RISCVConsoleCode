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


int kiet_nn_get_random_mod(nn_t out, nn_src_t q)
{
	nn tmp_rand, qprime;
	bitcnt_t q_bit_len, q_len;
	int ret, isone;
	qprime.magic = tmp_rand.magic = WORD(0);

	/* Check q is initialized and get its bit length */
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_bitlen(q, &q_bit_len); EG(ret, err);
	q_len = (bitcnt_t)BYTECEIL(q_bit_len);

	/* Check q is neither 0, nor 1 and its size is ok */
	MUST_HAVE((q_len) && (q_len <= (NN_MAX_BYTE_LEN / 2)), ret, err);
	MUST_HAVE((!nn_isone(q, &isone)) && (!isone), ret, err);

	/* 1) compute q' = q - 1  */
	ret = nn_copy(&qprime, q); EG(ret, err);
	ret = nn_dec(&qprime, &qprime); EG(ret, err);

	/* 2) generate a random value tmp_rand twice the size of q */
	ret = nn_init(&tmp_rand, (u16)(2 * q_len)); EG(ret, err);
	ret = get_random((u8 *)tmp_rand.val, (u16)(2 * q_len)); EG(ret, err);

	/* 3) compute out = tmp_rand mod q' */
	ret = nn_init(out, (u16)q_len); EG(ret, err);

	/* Use nn_mod_notrim to avoid exposing the generated random length */
    long start = (long) read_csr(mcycle);
	ret = nn_mod_notrim(out, &tmp_rand, &qprime); EG(ret, err);
    long end = (long) read_csr(mcycle);
    kprintf("==> modulo cycle: %d\n", end - start);

	/* 4) compute out += 1 */
	ret = nn_inc(out, out);

 err:
	nn_uninit(&qprime);
	nn_uninit(&tmp_rand);

	return ret;
}

int kiet_prj_pt_mul(prj_pt_t out, nn_src_t m, prj_pt_src_t in){
    int ret, on_curve;

	ret = prj_pt_check_initialized(in); EG(ret, err);
	ret = nn_check_initialized(m); EG(ret, err);

	/* Check that the input is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(in, &on_curve)) && on_curve, ret, err);

    long start = (long) read_csr(mcycle);
    ret = kiet_prj_pt_mul_ltr_monty_ladder(out, m, in); EG(ret, err);
    long end = (long) read_csr(mcycle);
    kprintf("==> point scalar mult: %d\n", end - start);

	/* Check that the output is on the curve */
	MUST_HAVE((!prj_pt_is_on_curve(out, &on_curve)) && on_curve, ret, err);

err:
	return ret;
}

static int kiet_prj_pt_mul_ltr_monty_ladder(prj_pt_t out, nn_src_t m, prj_pt_src_t in)
{
	/* We use Itoh et al. notations here for T and the random r */
	prj_pt T[3];
	bitcnt_t mlen;
	u8 mbit, rbit;
	/* Random for masking the Montgomery Ladder algorithm */
	nn r;
	/* The new scalar we will use with MSB fixed to 1 (noted m' above).
	 * This helps dealing with constant time.
	 */
	nn m_msb_fixed;
	nn_src_t curve_order;
	nn curve_order_square;
	int ret, ret_ops, cmp;
	r.magic = m_msb_fixed.magic = curve_order_square.magic = WORD(0);
	T[0].magic = T[1].magic = T[2].magic = WORD(0);

	/* Compute m' from m depending on the rule described above */
	curve_order = &(in->crv->order);

	/* First compute q**2 */
	ret = nn_sqr(&curve_order_square, curve_order); EG(ret, err);

	/* Then compute m' depending on m size */
	ret = nn_cmp(m, curve_order, &cmp); EG(ret, err);
	if (cmp < 0) {
		bitcnt_t msb_bit_len, order_bitlen;

		/* Case where m < q */
		ret = nn_add(&m_msb_fixed, m, curve_order); EG(ret, err);
		ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
		ret = nn_bitlen(curve_order, &order_bitlen); EG(ret, err);
		ret = nn_cnd_add((msb_bit_len == order_bitlen), &m_msb_fixed,
				&m_msb_fixed, curve_order); EG(ret, err);
	} else {
		ret = nn_cmp(m, &curve_order_square, &cmp); EG(ret, err);
		if (cmp < 0) {
			bitcnt_t msb_bit_len, curve_order_square_bitlen;

			/* Case where m >= q and m < (q**2) */
			ret = nn_add(&m_msb_fixed, m, &curve_order_square); EG(ret, err);
			ret = nn_bitlen(&m_msb_fixed, &msb_bit_len); EG(ret, err);
			ret = nn_bitlen(&curve_order_square, &curve_order_square_bitlen); EG(ret, err);
			ret = nn_cnd_add((msb_bit_len == curve_order_square_bitlen),
					 &m_msb_fixed, &m_msb_fixed, &curve_order_square); EG(ret, err);
		} else {
			/* Case where m >= (q**2) */
			ret = nn_copy(&m_msb_fixed, m); EG(ret, err);
		}
	}

	ret = nn_bitlen(&m_msb_fixed, &mlen); EG(ret, err);
	MUST_HAVE((mlen != 0), ret, err);
	mlen--;

	/* Hide possible internal failures for double and add
	 * operations and perform the operation in constant time.
	 */
	ret_ops = 0;

	/* Get a random r with the same size of m_msb_fixed */
	ret = nn_get_random_len(&r, (u16)(m_msb_fixed.wlen * WORD_BYTES)); EG(ret, err);

	ret = nn_getbit(&r, mlen, &rbit); EG(ret, err);

	/* Initialize points */
	ret = prj_pt_init(&T[0], in->crv); EG(ret, err);
	ret = prj_pt_init(&T[1], in->crv); EG(ret, err);
	ret = prj_pt_init(&T[2], in->crv); EG(ret, err);

	/* Initialize T[r[n-1]] to input point */
	/*
	 * Blind the point with projective coordinates
	 * (X, Y, Z) => (l*X, l*Y, l*Z)
	 */
	ret = kiet_blind_projective_point(&T[rbit], in); EG(ret, err);

	/* Initialize T[1-r[n-1]] with ECDBL(T[r[n-1]])) */
#ifndef NO_USE_COMPLETE_FORMULAS
	/*
	 * NOTE: in case of complete formulas, we use the
	 * addition for doubling, incurring a small performance hit
	 * for better SCA resistance.
	 */
	ret_ops |= prj_pt_add(&T[1-rbit], &T[rbit], &T[rbit]);
#else
	ret_ops |= prj_pt_dbl(&T[1-rbit], &T[rbit]);
#endif

	/* Main loop of the Montgomery Ladder */
	while (mlen > 0) {
		u8 rbit_next;
		--mlen;
		/* rbit is r[i+1], and rbit_next is r[i] */
		ret = nn_getbit(&r, mlen, &rbit_next); EG(ret, err);

		/* mbit is m[i] */
		ret = nn_getbit(&m_msb_fixed, mlen, &mbit); EG(ret, err);
		/* Double: T[2] = ECDBL(T[d[i] ^ r[i+1]]) */

#ifndef NO_USE_COMPLETE_FORMULAS
		/* NOTE: in case of complete formulas, we use the
		 * addition for doubling, incurring a small performance hit
		 * for better SCA resistance.
		 */
		ret_ops |= prj_pt_add(&T[2], &T[mbit ^ rbit], &T[mbit ^ rbit]);
#else
		ret_ops |= prj_pt_dbl(&T[2], &T[mbit ^ rbit]);
#endif

		/* Add: T[1] = ECADD(T[0],T[1]) */
		ret_ops |= prj_pt_add(&T[1], &T[0], &T[1]);

		/* T[0] = T[2-(d[i] ^ r[i])] */
		/*
		 * NOTE: we use the low level nn_copy function here to avoid
		 * any possible leakage on operands with prj_pt_copy
		 */
		ret = nn_copy(&(T[0].X.fp_val), &(T[2-(mbit ^ rbit_next)].X.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[0].Y.fp_val), &(T[2-(mbit ^ rbit_next)].Y.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[0].Z.fp_val), &(T[2-(mbit ^ rbit_next)].Z.fp_val)); EG(ret, err);

		/* T[1] = T[1+(d[i] ^ r[i])] */
		/* NOTE: we use the low level nn_copy function here to avoid
		 * any possible leakage on operands with prj_pt_copy
		 */
		ret = nn_copy(&(T[1].X.fp_val), &(T[1+(mbit ^ rbit_next)].X.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[1].Y.fp_val), &(T[1+(mbit ^ rbit_next)].Y.fp_val)); EG(ret, err);
		ret = nn_copy(&(T[1].Z.fp_val), &(T[1+(mbit ^ rbit_next)].Z.fp_val)); EG(ret, err);

		/* Update rbit */
		rbit = rbit_next;
	}
	/* Output: T[r[0]] */
	ret = prj_pt_copy(out, &T[rbit]); EG(ret, err);

	/* Take into consideration our double and add errors */
	ret |= ret_ops;

err:
	prj_pt_uninit(&T[0]);
	prj_pt_uninit(&T[1]);
	prj_pt_uninit(&T[2]);
	nn_uninit(&r);
	nn_uninit(&m_msb_fixed);
	nn_uninit(&curve_order_square);

	PTR_NULLIFY(curve_order);

	return ret;
}

static int kiet_blind_projective_point(prj_pt_t out, prj_pt_src_t in)
{
	int ret;

	/* Random for projective coordinates masking */
	/* NOTE: to limit stack usage, we reuse out->Z as a temporary
	 * variable. This does not work if in == out, hence the check.
	 */
	MUST_HAVE((in != out), ret, err);

	ret = prj_pt_init(out, in->crv); EG(ret, err);

	/* Get a random value l in Fp */
	ret = fp_get_random(&(out->Z), in->X.ctx); EG(ret, err);

	/*
	 * Blind the point with projective coordinates
	 * (X, Y, Z) => (l*X, l*Y, l*Z)
	 */
	ret = fp_mul_monty(&(out->X), &(in->X), &(out->Z)); EG(ret, err);
	ret = fp_mul_monty(&(out->Y), &(in->Y), &(out->Z)); EG(ret, err);
	ret = fp_mul_monty(&(out->Z), &(in->Z), &(out->Z));

err:
	return ret;
}


int kiet_prj_pt_add(prj_pt_t out, prj_pt_src_t in1, prj_pt_src_t in2)
{
	int ret;

	ret = prj_pt_check_initialized(in1); EG(ret, err);
	ret = prj_pt_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->crv == in2->crv), ret, err);

    long start = (long) read_csr(mcycle);
    ret = kiet__prj_pt_add_monty_cf(out, in1, in2);
    long end = (long) read_csr(mcycle);
    kprintf("==> point addition: %d\n", end - start);
err:
	return ret;
}

static int kiet__prj_pt_add_monty_cf(prj_pt_t out,
							   prj_pt_src_t in1,
							   prj_pt_src_t in2)
{
	int cmp1, cmp2;
	fp t0, t1, t2, t3, t4, t5;
	int ret;
	t0.magic = t1.magic = t2.magic = WORD(0);
	t3.magic = t4.magic = t5.magic = WORD(0);

	ret = prj_pt_init(out, in1->crv); EG(ret, err);

	ret = fp_init(&t0, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t1, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t2, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t3, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t4, out->crv->a.ctx); EG(ret, err);
	ret = fp_init(&t5, out->crv->a.ctx); EG(ret, err);

	ret = fp_mul_monty(&t0, &in1->X, &in2->X); EG(ret, err);
	ret = fp_mul_monty(&t1, &in1->Y, &in2->Y); EG(ret, err);
	ret = fp_mul_monty(&t2, &in1->Z, &in2->Z); EG(ret, err);
	ret = fp_add_monty(&t3, &in1->X, &in1->Y); EG(ret, err);
	ret = fp_add_monty(&t4, &in2->X, &in2->Y); EG(ret, err);

	ret = fp_mul_monty(&t3, &t3, &t4); EG(ret, err);
	ret = fp_add_monty(&t4, &t0, &t1); EG(ret, err);
	ret = fp_sub_monty(&t3, &t3, &t4); EG(ret, err);
	ret = fp_add_monty(&t4, &in1->X, &in1->Z); EG(ret, err);
	ret = fp_add_monty(&t5, &in2->X, &in2->Z); EG(ret, err);

	ret = fp_mul_monty(&t4, &t4, &t5); EG(ret, err);
	ret = fp_add_monty(&t5, &t0, &t2); EG(ret, err);
	ret = fp_sub_monty(&t4, &t4, &t5); EG(ret, err);
	ret = fp_add_monty(&t5, &in1->Y, &in1->Z); EG(ret, err);
	ret = fp_add_monty(&out->X, &in2->Y, &in2->Z); EG(ret, err);

	ret = fp_mul_monty(&t5, &t5, &out->X); EG(ret, err);
	ret = fp_add_monty(&out->X, &t1, &t2); EG(ret, err);
	ret = fp_sub_monty(&t5, &t5, &out->X); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &in1->crv->a_monty, &t4); EG(ret, err);
	ret = fp_mul_monty(&out->X, &in1->crv->b3_monty, &t2); EG(ret, err);

	ret = fp_add_monty(&out->Z, &out->X, &out->Z); EG(ret, err);
	ret = fp_sub_monty(&out->X, &t1, &out->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &t1, &out->Z); EG(ret, err);
	ret = fp_mul_monty(&out->Y, &out->X, &out->Z); EG(ret, err);
	ret = fp_add_monty(&t1, &t0, &t0); EG(ret, err);

	ret = fp_add_monty(&t1, &t1, &t0); EG(ret, err);
	ret = fp_mul_monty(&t2, &in1->crv->a_monty, &t2); EG(ret, err);
	ret = fp_mul_monty(&t4, &in1->crv->b3_monty, &t4); EG(ret, err);
	ret = fp_add_monty(&t1, &t1, &t2); EG(ret, err);
	ret = fp_sub_monty(&t2, &t0, &t2); EG(ret, err);

	ret = fp_mul_monty(&t2, &in1->crv->a_monty, &t2); EG(ret, err);
	ret = fp_add_monty(&t4, &t4, &t2); EG(ret, err);
	ret = fp_mul_monty(&t0, &t1, &t4); EG(ret, err);
	ret = fp_add_monty(&out->Y, &out->Y, &t0); EG(ret, err);
	ret = fp_mul_monty(&t0, &t5, &t4); EG(ret, err);

	ret = fp_mul_monty(&out->X, &t3, &out->X); EG(ret, err);
	ret = fp_sub_monty(&out->X, &out->X, &t0); EG(ret, err);
	ret = fp_mul_monty(&t0, &t3, &t1); EG(ret, err);
	ret = fp_mul_monty(&out->Z, &t5, &out->Z); EG(ret, err);
	ret = fp_add_monty(&out->Z, &out->Z, &t0);

	/* Check for "exceptional" pairs of input points with
	 * checking if Y = Z = 0 as output (see the Bosma-Lenstra
	 * article "Complete Systems of Two Addition Laws for
	 * Elliptic Curves"). This should only happen on composite
	 * order curves (i.e. not on prime order curves).
	 *
	 * In this case, we raise an error as the result is
	 * not sound. This should not happen in our nominal
	 * cases with regular signature and protocols, and if
	 * it happens this usually means that bad points have
	 * been injected.
	 *
	 * NOTE: if for some reasons you need to deal with
	 * all the possible pairs of points including these
	 * exceptional pairs of inputs with an order 2 difference,
	 * you should fallback to the incomplete formulas using the
	 * COMPLETE=0 compilation toggle. Beware that in this
	 * case, the library will be more sensitive to
	 * side-channel attacks.
	 */
	ret = fp_iszero(&(out->Z), &cmp1); EG(ret, err);
	ret = fp_iszero(&(out->Y), &cmp2); EG(ret, err);
	MUST_HAVE(!((cmp1) && (cmp2)), ret, err);

err:
	fp_uninit(&t0);
	fp_uninit(&t1);
	fp_uninit(&t2);
	fp_uninit(&t3);
	fp_uninit(&t4);
	fp_uninit(&t5);

	return ret;
}