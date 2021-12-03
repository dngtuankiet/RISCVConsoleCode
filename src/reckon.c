#include "platform.h"
#include "main.h"
#include "i2c/i2c.h"
#include "clkutils.h"
#include <kprintf/kprintf.h>
#include "codec/codec.h"
#include "FFT_int/fix_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

void cdft(int, int, double *, int *, double *);

#ifdef __cplusplus
}
#endif

#ifndef NMAX
#define NMAX 8192
#define NMAXSQRT 64
#define NMAXM 13
#endif

int reckon_start(void){
  i2c0_init((void*)i2c_reg, 1000000, METAL_I2C_MASTER);
  int32_t l[NMAX/2];
  int32_t r[NMAX/2];
  double a[NMAX + 1], w[NMAX * 5 / 4], t[NMAX / 2 + 1];
  int n = NMAX/2, ip[NMAXSQRT + 2];
  int m = NMAXM - 1;
  short fft[NMAX];
  double fftd[NMAX];
  uint64_t start, endfft, endfftd;
  // Init the codec in 32-bit, 96KHz
  codec_init((void*)i2c_reg, CODEC_WORD_LENGTH_32B, CODEC_FORMAT_LEFT, CODEC_SAMPLING_ADC_96KHZ_DAC_96KHZ);
  while(1) {
    codec_sample_now((void*)codec_reg, l, r, n, CODEC_MASK_32B);
    // Convert to 16-bit
    memset(fft, 0, NMAX*sizeof(short));
    for (int k = 0 ; k < n ; k++)
    {
      // The channel is the left one
      fft[k] = (short)(l[k] >> 16);
    }
    // Convert to double
    memset(fftd, 0, NMAX*sizeof(double));
    for (int k = 0 ; k < n ; k++)
    {
      // The channel is the left one
      fftd[k*2] = ((double)(l[k] >> 16) / 32768.0);
      //fftd[k*2+1], which is the imaginary part, is zero
    }
    
    // Do fixed fft
    start = clkutils_read_mtime();
    fix_fft(fft, fft+n, m, 0);
    endfft = clkutils_read_mtime() - start;
    
    // Do floating fft
    start = clkutils_read_mtime();
    cdft(m, 1, fftd, ip, w);
    endfftd = clkutils_read_mtime() - start;
    
    kprintf("\r%i %i %i %i %i %i %i %i (%i %i)", 
        l[0], l[NMAX/2/4*1], l[NMAX/2/4*2], l[NMAX/2/4*3], 
        (int)fft[0], (int)fft[NMAX/2/4*1], (int)fft[NMAX/2/4*2], (int)fft[NMAX/2/4*2], 
        (int)endfft, (int)endfftd);
  }
}
