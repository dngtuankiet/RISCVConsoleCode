
#ifndef _XPR_H
#define _XPR_H

#define XPR_CTRL        0x00
#define XPR_STATUS      0x04
#define XPR_DELAY       0x08
#define XPR_RANDOM      0x0C
#define XPR_IR          0x10 
#define XPR_I1          0x14
#define XPR_I2          0x18
#define XPR_PUF         0x1C
#define XPR_SEED        0x20
#define XPR_RG_STATE    0x24
#define XPR_MASK        0x28





//XPR_CTRL control register
#define XPR_CTRL_ENABLE (0x1<<0)
#define XPR_CTRL_NEXT   (0x1<<1)
#define XPR_CTRL_INIT   (0x1<<2)
#define XPR_CTRL_PUF_MODE   (0x1<<3)
#define XPR_CTRL_RANDOM_MODE   (0x0<<3)

#define XPR_CTRL_RESET  (0x1<<8)

//XPR_STATUS
#define XPR_STAT_VALID  (0x1<<0)

//XPR_PAIR
#define XPR_PAIR_0      (0x1<<0)
#define XPR_PAIR_1      (0x1<<1)
#define XPR_PAIR_2      (0x1<<2)
#define XPR_PAIR_3      (0x1<<3)
#define XPR_PAIR_4      (0x1<<4)
#define XPR_PAIR_5      (0x1<<5)
#define XPR_PAIR_6      (0x1<<6)
#define XPR_PAIR_7      (0x1<<7)
#define XPR_PAIR_8      (0x1<<8)
#define XPR_PAIR_9      (0x1<<9)
#define XPR_PAIR_10     (0x1<<10)
#define XPR_PAIR_11     (0x1<<11)
#define XPR_PAIR_12     (0x1<<12)






#endif /* _XPR_H */