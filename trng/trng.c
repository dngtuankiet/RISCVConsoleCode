#include <platform.h>
#include "trng.h"

void trng_reset(void* trngctrl){
  //reset high, reset both ring generator and ring oscillator
  _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_RESET;
}

void trng_reset_disable(void* trngctrl){
    _REG32((char*)trngctrl, TRNG_CONTROL) = TRNG_RESET & (~TRNG_ENABLE);
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
    _REG32((char*)trngctrl, TRNG_CONTROL) = _REG32((char*)trngctrl, TRNG_CONTROL) | TRNG_ENABLE;
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
}

uint32_t trng_get_random(void* trngctrl){
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