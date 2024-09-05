#include "platform.h"
#include "devices/xpr.h"
#include "xpr_driver.h"

void xpr_reset(void* xpr_reg){
    //reset high, reset the ring_gengerator_base by XPR_CTRL_RESET
    //reset low, reset the xpr_slice by XPR_CTRL_IR = 0
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | (XPR_CTRL_RESET);
    _REG32((char*)xpr_reg, XPR_I1) = 0;
    _REG32((char*)xpr_reg, XPR_I2) = 0;
    _REG32((char*)xpr_reg, XPR_IR) = 0;

    //check RG_STATE
    kprintf("RG_STATE (should be 0): %d\n", _REG32((char*)xpr_reg, XPR_RG_STATE));
}

void xpr_reset_and_disable(void* xpr_reg){
    //reset high, reset the ring_gengerator_base by XPR_CTRL_RESET
    //reset low, reset the xpr_slice by XPR_CTRL_IR = 0
    _REG32((char*)xpr_reg, XPR_CTRL) = XPR_CTRL_RESET;
    _REG32((char*)xpr_reg, XPR_I1) = 0;
    _REG32((char*)xpr_reg, XPR_I2) = 0;
    _REG32((char*)xpr_reg, XPR_IR) = 0;

    //check RG_STATE
    // kprintf("RG_STATE (should be 0): %x\n", _REG32((char*)xpr_reg, XPR_RG_STATE));
}

int xpr_setup(void* xpr_reg, uint32_t delay, uint32_t pair_selection){
    // uint32_t reg = 0;

    xpr_reset_and_disable(xpr_reg);
    _REG32((char*)xpr_reg, XPR_CTRL) = 0x0; //release reset signal of the ring_gengerator_base

    //set delay time
    _REG32((char*)xpr_reg, XPR_DELAY) = delay;
    #ifdef XPR_DEBUG
    reg = _REG32(xpr_reg, XPR_DELAY);
    kprintf("XPR-set delay: %d \n", reg);
    #endif //XPR_DEBUG

    //Enable the ring_gengerator_base
    _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | (XPR_CTRL_ENABLE);
    //Enable the ring_gengerator_base and trigger the oscillation mode of xpr_slice
    _REG32((char*)xpr_reg, XPR_I1) = pair_selection;
    // _REG32((char*)xpr_reg, XPR_I2) = ~pair_selection;

    //Enable oscillation mode of xpr_slice
    _REG32((char*)xpr_reg, XPR_IR) = pair_selection;


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
    // uint32_t reg = 0;
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

int xpr_xor_puf_trigger1(void* xpr_reg, uint32_t delay, uint32_t pair_selection){
    uint32_t puf = 0;

    xpr_reset_and_disable(xpr_reg);
    _REG32((char*)xpr_reg, XPR_CTRL) = 0x0; //release reset signal of the ring_gengerator_base
    _REG32((char*)xpr_reg, XPR_CTRL) = XPR_CTRL_PUF_MODE;

    _REG32((char*)xpr_reg, XPR_IR) = 0;
    _REG32((char*)xpr_reg, XPR_I1) = 0;
    _REG32((char*)xpr_reg, XPR_I2) = 0;
    _REG32((char*)xpr_reg, XPR_IR) = pair_selection;

    //trigger xor_puf
    _REG32((char*)xpr_reg, XPR_I1) = pair_selection;
    _REG32((char*)xpr_reg, XPR_I2) = pair_selection;

    for(int i = 0; i <= 10000; i++){
    }

    puf = _REG32((char*)xpr_reg, XPR_PUF);

    return puf;
}

int xpr_xor_puf_trigger2(void* xpr_reg, uint32_t delay, uint32_t pair_selection){
    uint32_t puf = 0;

    xpr_reset_and_disable(xpr_reg);
    _REG32((char*)xpr_reg, XPR_CTRL) = 0x0; //release reset signal of the ring_gengerator_base
    _REG32((char*)xpr_reg, XPR_CTRL) = XPR_CTRL_PUF_MODE;

    _REG32((char*)xpr_reg, XPR_IR) = 0;
    _REG32((char*)xpr_reg, XPR_I1) = 0;
    _REG32((char*)xpr_reg, XPR_I2) = 0;

    //trigger xor_puf
    _REG32((char*)xpr_reg, XPR_I1) = pair_selection;
    _REG32((char*)xpr_reg, XPR_I2) = pair_selection;

    _REG32((char*)xpr_reg, XPR_IR) = pair_selection;

    for(int i = 0; i <= 10000; i++){
    }

    puf = _REG32((char*)xpr_reg, XPR_PUF);

    return puf;
}


int xpr_puf_mode(void * xpr_reg, uint32_t delay, uint32_t pair_selection, uint32_t challenge){
    uint32_t rgState = 0;
    uint32_t pufOG = 0;
    uint32_t xprPUF = 0;
    uint32_t reg = 0;
    int max = 0;


    xpr_reset_and_disable(xpr_reg);
    
    //ARM the XPR
    pufOG = xpr_xor_puf_trigger1((void*)xpr_reg, delay, pair_selection);
    kprintf("PUF OG value - should be different from 0: %x\n", pufOG);

    //Init the Challenge 
    //Enable the RG - Can check the state of the RG here
    _REG32((char*)xpr_reg, XPR_SEED) = challenge;
    reg = _REG32((char*)xpr_reg, XPR_CTRL);
    _REG32((char*)xpr_reg, XPR_CTRL) = reg | (XPR_CTRL_ENABLE | XPR_CTRL_INIT);
    
    rgState = _REG32((char*)xpr_reg, XPR_RG_STATE);
    kprintf("Ring Generator state - should be the same as Challenge: %x\n", rgState);

    //Disable Init
    //Wait for calibration
    reg = _REG32((char*)xpr_reg, XPR_CTRL);
    // while(!((_REG32((char*)xpr_reg, XPR_STATUS) & XPR_STAT_VALID) == XPR_STAT_VALID)){
    //     max = max + 1;
    //     if(max == MAX_WAIT_TIME){
    //         kprintf("Valid should be 0\n");
    //     }
    // }
    _REG32((char*)xpr_reg, XPR_CTRL) = reg & (~XPR_CTRL_INIT);
    reg = _REG32((char*)xpr_reg, XPR_CTRL);
    kprintf("Check CTRL: %x\n", reg);

    //Readout
    while(!((_REG32((char*)xpr_reg, XPR_STATUS) & XPR_STAT_VALID) == XPR_STAT_VALID)){
        max = max + 1;
        if(max == MAX_WAIT_TIME){
            kprintf("XPR-error waiting calibration\n");
            return XPR_ERROR_WAIT;
        }
    }

    kprintf("Check Status: %x\n", _REG32((char*)xpr_reg, XPR_STATUS));
    xprPUF = _REG32((char*)xpr_reg, XPR_RANDOM);

    return xprPUF;
}


// uint32_t xpr_get_puf(void* xpr_reg){
//     // uint32_t reg = 0;
//     uint32_t puf = 0;
//     _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) & (~XPR_CTRL_NEXT);
//     _REG32((char*)xpr_reg, XPR_CTRL) = _REG32((char*)xpr_reg, XPR_CTRL) | XPR_CTRL_NEXT;
//     #ifdef XPR_DEBUG
//     reg = _REG32(xpr_reg, XPR_CTRL);
//     kprintf("XPR-set control: %d \n", reg);
//     #endif //XPR_DEBUG

//     int max = 0;
//     while(!((_REG32((char*)xpr_reg, XPR_STATUS) & XPR_STAT_VALID) == XPR_STAT_VALID)){
//         max = max + 1;
//         if(max == MAX_WAIT_TIME){
//             kprintf("XPR-error waiting random\n");
//             return XPR_ERROR_RANDOM;
//         }
//     }

//     puf =  _REG32((char*)xpr_reg, XPR_RANDOM);
//     return puf;
// }