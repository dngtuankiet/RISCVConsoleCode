// See LICENSE for license details.

#ifndef _RATONA_FFT_H
#define _RATONA_FFT_H

/* Register offsets */

#define FFT_REG_DATA_IN       0x00
#define FFT_REG_DATA_OUT      0x04
#define FFT_REG_ADDR_IN       0x08
#define FFT_REG_ADDR_OUT      0x0c
#define FFT_REG_CTRL          0x10
#define FFT_REG_STATUS        0x14

/* Fields */
#define FFT_CTRL_START (1UL << 0)
#define FFT_CTRL_WR_IN (1UL << 1)
#define FFT_CTRL_SYN_RST (1UL << 2)

#define FFT_STAT_READY (1UL << 0)
#define FFT_STAT_BUSY (1UL << 1)

#define LOG2_FFT_LEN 8 // TODO: Depends of core

#endif /* _RATONA_CODEC_H */
