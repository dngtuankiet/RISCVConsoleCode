#ifndef _MAIN_H
#define _MAIN_H

extern unsigned long uart_reg;
extern int timescale_freq;
extern int tlclk_freq;

extern unsigned long i2c_reg;
extern unsigned long codec_reg;
extern unsigned long fft_reg;
extern unsigned long fftdma_reg;

int reckon_start(void);

#endif // _MAIN_H
