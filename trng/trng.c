#include "trng.h"

#ifdef CUSTOM_LIBECC
int my_rng_gen_block(unsigned char* buf, uint16_t len){
    int ret = 0;
	uint16_t need = len, copied = 0;
	uint16_t count = 4;

	unsigned int rand = 0;
    uint8_t buffer[4];

	if (need <= 0) {
		return -1;
	}
    while(need != copied){
        rand = trng_get_random((void*)trng_reg);
        if(rand == 0){
            ret = -1;
            goto end;
        }
        *(uint32_t*)buffer = rand;
		memcpy((buf+copied), buffer, count);
		copied = copied + count;
		if((need-copied) < 4){
			count = need-copied;
		}else{
			count = 4;
		}
	}

end:
    return ret;
}
#endif //CUSTOM_LIBECC

unsigned int my_rng_seed_gen(void){
    return trng_get_random((void*)trng_reg);
}

void trng_reset(void* trngctrl){
  //reset high, reset both ring generator and ring oscillator
  _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_RESET;
}

void trng_reset_disable(void* trngctrl){
    _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_RESET_AND_DISABLE_MODE;
}

void trng_disable_mode(void* trngctrl){
    _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_DISABLE_MODE;
}

void trng_enable_mode(void* trngctrl){
    _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_ENABLE_MODE;
}


/**
 * Setup trng
 * Input: delay time (number of cycles, default: 2^11)
*/
int trng_setup(void* trngctrl, uint32_t delay){
    uint32_t reg = 0;

    trng_reset(trngctrl);
    _REG32((char*)trngctrl, TRNG_CONTROL) = 0x0; //release reset signal

    //set delay time
    _REG32((char*)trngctrl, TRNG_DELAY) = delay;
    #ifdef TRNG_DEBUG
    reg = _REG32(trngctrl, TRNG_DELAY);
    kprintf("TRNG-set delay: %d \n", reg);
    #endif //TRNG_DEBUG

    //enable module
    // _REG32((char*)trngctrl, TRNG_CONTROL) = _REG32((char*)trngctrl, TRNG_CONTROL) | TRNG_ENABLE;
    trng_enable_mode(trngctrl);
    #ifdef TRNG_DEBUG
    reg = _REG32(trngctrl, TRNG_CONTROL);
    kprintf("TRNG-set control: %d \n", reg);
    #endif //TRNG_DEBUG

    #ifdef TRNG_DEBUG
    kprintf("TRNG-start waiting calibration\n");
    #endif //TRNG_DEBUG

    int max = 0;
    while(!((_REG32((char*)trngctrl, TRNG_STATUS) & TRNG_VALID_BIT) == TRNG_VALID_BIT)){
        max = max + 1;
        if(max == MAX_WAIT_TIME){
            kprintf("TRNG-error waiting calibration\n");
            return TRNG_ERROR_WAIT;
        }
    }
    #ifdef TRNG_DEBUG
    kprintf("TRNG - initialization completed\n");
    #endif //TRNG_DEBUG
    
    //checking first random
    uint32_t rand = _REG32((char*)trngctrl, TRNG_RANDOM);
    if(rand == 0){
        kprintf("TRNG-error gen random number\n");
        return TRNG_ERROR_RANDOM;
    }
    
    trng_disable_mode(trngctrl);

    return TRNG_OKAY;
}

uint32_t trng_get_random(void* trngctrl){
    trng_enable_mode(trngctrl);

    uint32_t reg = 0;
    uint32_t rand = 0;
    _REG32((char*)trngctrl, TRNG_CONTROL) = _REG32((char*)trngctrl, TRNG_CONTROL) & (~TRNG_NEXT);
    _REG32((char*)trngctrl, TRNG_CONTROL) = _REG32((char*)trngctrl, TRNG_CONTROL) | TRNG_NEXT;
    #ifdef TRNG_DEBUG
    reg = _REG32(trngctrl, TRNG_CONTROL);
    kprintf("TRNG-set control: %d \n", reg);
    #endif //TRNG_DEBUG

    int max = 0;
    while(!((_REG32((char*)trngctrl, TRNG_STATUS) & TRNG_VALID_BIT) == TRNG_VALID_BIT)){
        max = max + 1;
        if(max == MAX_WAIT_TIME){
            kprintf("TRNG-error waiting random\n");
            return TRNG_ERROR_RANDOM;
        }
    }

    rand =  _REG32((char*)trngctrl, TRNG_RANDOM);
    return rand;
}