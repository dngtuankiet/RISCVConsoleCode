#include "platform.h"
#include "devices/xpr.h"
#include "xpr_driver.h"

void xpr_reset(void* xpr_reg){
    //reset high, reset the ring_gengerator_base by XPR_CTRL_RESET
    //reset low, reset the xpr_slice by XPR_CTRL_IR = 0
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | (XPR_CTRL_RESET);
}

void xpr_reset_and_disable(void* xpr_reg){
    //reset high, reset the ring_gengerator_base by XPR_CTRL_RESET
    //reset low, reset the xpr_slice by XPR_CTRL_IR = 0
    _REG32((char*)xpr_reg, XPR_CTRL) = XPR_CTRL_RESET;
}

int xpr_setup(void* xpr_reg, uint32_t delay){
    uint32_t reg = 0;

    xpr_reset_and_disable(xpr_reg);
    _REG32((char*)xpr_reg, XPR_CTRL) = 0x0; //release reset signal of the ring_gengerator_base

    //set delay time
    _REG32((char*)xpr_reg, XPR_DELAY) = delay;
    #ifdef XPR_DEBUG
    reg = _REG32(xpr_reg, XPR_DELAY);
    kprintf("XPR-set delay: %d \n", reg);
    #endif //XPR_DEBUG

    //Enable the ring_gengerator_base and trigger the oscillation mode of xpr_slice
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | (XPR_CTRL_ENABLE | XPR_CTRL_I1 | XPR_CTRL_IR);
    #ifdef XPR_DEBUG
    reg = _REG32(xpr_reg, XPR_CTRL);
    kprintf("XPR-set control: %d \n", reg);
    #endif //XPR_DEBUG

    #ifdef XPR_DEBUG
    kprintf("XPR-start waiting calibration\n");
    #endif //XPR_DEBUG

    int max = 0;
    while(!((_REG32((char*)xpr_reg, XPR_STATUS) & XPR_STAT_VALID) == XPR_STAT_VALID)){
        max = max + 1;
        if(max == MAX_WAIT_TIME){
            kprintf("XPR-error waiting calibration\n");
            return XPR_ERROR_WAIT;
        }
    }
    #ifdef XPR_DEBUG
    kprintf("XPR - initialization completed\n");
    #endif //XPR_DEBUG
    
    //checking first random
    uint32_t rand = _REG32((char*)xpr_reg, XPR_RANDOM);
    if(rand == 0){
        kprintf("XPR-error gen random number\n");
        return XPR_ERROR_RANDOM;
    }
}


uint32_t xpr_get_random(void* xpr_reg){
    uint32_t reg = 0;
    uint32_t rand = 0;
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) & (~XPR_CTRL_NEXT);
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | XPR_CTRL_NEXT;
    #ifdef XPR_DEBUG
    reg = _REG32(xpr_reg, XPR_CTRL);
    kprintf("XPR-set control: %d \n", reg);
    #endif //XPR_DEBUG

    int max = 0;
    while(!((_REG32((char*)xpr_reg, XPR_STATUS) & XPR_STAT_VALID) == XPR_STAT_VALID)){
        max = max + 1;
        if(max == MAX_WAIT_TIME){
            kprintf("XPR-error waiting random\n");
            return XPR_ERROR_RANDOM;
        }
    }

    rand =  _REG32((char*)xpr_reg, XPR_RANDOM);
    return rand;
}
