
#ifndef _XPR_H
#define _XPR_H

#define XPR_CTRL        0x00
#define XPR_STATUS      0x04
#define XPR_DELAY       0x08
#define XPR_RANDOM      0x0C

#define XPR_CTRL_ENABLE (0x1<<0)
#define XPR_CTRL_NEXT   (0x1<<1)
#define XPR_CTRL_IR     (0x1<<3)
#define XPR_CTRL_I1     (0x1<<4)
#define XPR_CTRL_I2     (0x1<<5)
#define XPR_CTRL_RESET  (0x1<<8)

#define XPR_STAT_VALID  (0x1<<0)

#endif /* _XPR_H */