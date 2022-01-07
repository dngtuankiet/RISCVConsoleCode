#include "platform.h"
#include "main.h"
#include "i2c/i2c.h"
#include "clkutils.h"
#include <kprintf/kprintf.h>
#include <stdlib.h>
#include "codec/codec.h"
#include "FFT_int/fix_fft.h"
#include <string.h>
#include "FFT_hw/fft_hw.h"
#include "MFCC/libmfcc.h"
#include <math.h>

// WARNING: Make them match
#define SAMPLING_RATE 48000
#define CODEC_SAMPLING CODEC_SAMPLING_ADC_48KHZ_DAC_48KHZ

#define MTOTM 16 // 2^16 samples
#define MTOT (1 << MTOTM)
#define HANNSIZE (1 << LOG2_FFT_LEN) // TODO: Save half the window, because simmetry
#define FRAMES (1 << (MTOTM - LOG2_FFT_LEN))

int32_t l[MTOT];
int32_t r[MTOT];
double rd[HANNSIZE];
int32_t hann[HANNSIZE];
#define NUM_MFCC 13
double mfcc[FRAMES][NUM_MFCC];

int reckon_start(void){
  // Preprocessing
  i2c0_init((void*)i2c_reg, 1000000, METAL_I2C_MASTER);
  codec_init((void*)i2c_reg, CODEC_WORD_LENGTH_32B, CODEC_FORMAT_LEFT, CODEC_SAMPLING);
  uint64_t start, end1 = 0, end2 = 0, end3 = 0, end4 = 0, end5 = 0, endt;
  
  // Pre-calculation of the Hann window
  // 0.54 - 0.46 * cos (2πn/(N-1))
  for(int i = 0; i < HANNSIZE; i++) {
    //        0.54               0.46
    hann[i] = 17695 - (int32_t)(15073.28*cos((float)(2*i)/(float)(HANNSIZE-1)));
  }
  // storing directly in the FFT vector
  volatile int32_t* dest = (volatile int32_t*)fftdma_reg;
  if(!dest) {
    dest = r;
  }
  
  while(1) {
    end3 = 0;
    end4 = 0;
    end5 = 0;
    // Sampling 1 second or so
    start = clkutils_read_mtime();
    codec_sample_now((void*)codec_reg, l, r, MTOT, CODEC_MASK_32B);
    end1 = clkutils_read_mtime() - start;
    
    // Filtering (also 32->16 bit convertion)
    // Filter = x[n] - (1 - 1/2^5) * x[n-1]
    start = clkutils_read_mtime();
    for(int i = 0; i < MTOT - 1; i++) {
      l[i] = (l[i+1] >> 16) - (l[i] >> 16) + (l[i] >> (16+5));
    }
    end2 = clkutils_read_mtime() - start;
    
    // Windowing
    // w(n) = 0.54 - 0.46 * cos (2πn/(N-1))
    for(int w = 0, frame = 0; w < (MTOT - HANNSIZE/2) ; w += HANNSIZE / 2, frame++ )
    {
      start = clkutils_read_mtime();
      for(int i = 0; i < HANNSIZE; i++){
        // Is the window current offset (w) + the sample (i)
        // multiplied by hann in the sample (i)
        // Displaced 16 because the result of the mult is 32, algebraic (>>>)
        dest[i] = l[w+i] * hann[i] >> 16; // TODO: Is not verified. Kinda sure this is not the way
      }
      endt = clkutils_read_mtime() - start;
      end3 += endt;
      
      start = clkutils_read_mtime();
      // FFT
#define HWDMA
#ifdef HWDMA      
      if(!fftdma_reg) {
        short imag[HANNSIZE];
        memset(imag, 0, HANNSIZE*sizeof(short));
        fft_hw((short*)dest, imag, LOG2_FFT_LEN, (void*)fft_reg);
      }
      else {
        _REG32(fft_reg, FFT_REG_CTRL) = FFT_CTRL_START;
        while(!(_REG32(fft_reg, FFT_REG_STATUS) & FFT_STAT_READY));
      }
#else
      short imag[HANNSIZE];
      memset(imag, 0, HANNSIZE*sizeof(short));
      fix_fft((short*)dest, imag, LOG2_FFT_LEN, 0);
#endif // HWDMA
      endt = clkutils_read_mtime() - start;
      end4 += endt;
      
      // MEL (or MFCC)
      // First, make the integers double
      // TODO: Do MFCC with integers only
      start = clkutils_read_mtime();
      for(int i = 0; i < HANNSIZE; i++) {
        short val = (short)(dest[i] & 0xffff); // First, get the 16-bit real-only part of the FFT
        rd[i] = (double)val/32768.0;
      }
      // Invoke the mfcc. Get the first 13 or so
      if(frame < FRAMES) for(int coeff = 0; coeff < 13; coeff++)
      {
        mfcc[frame][coeff] = GetCoefficient(rd, SAMPLING_RATE, 1, HANNSIZE, coeff);
        //kprintf("coef: %i\n", coeff);
      }
      endt = clkutils_read_mtime() - start;
      end5 += endt;
    
      //kputs("PARTIAL TIMES:\n");
      //kprintf("end1: %i\n", (int)end1);
      //kprintf("end2: %i\n", (int)end2);
      //kprintf("end3: %i\n", (int)end3);
      //kprintf("end4: %i\n", (int)end4);
      //kprintf("end5: %i\n", (int)end5);
      //kprintf("final: %i\n\n",  (int)(end1 + end2 + end3 + end4 + end5));
    }
    
    kputs("TIMES:\n");
    //kprintf("end1: %i\n", (int)end1);
    kprintf("end2: %i\n", (int)end2);
    kprintf("end3: %i\n", (int)end3);
    kprintf("end4: %i\n", (int)end4);
    kprintf("end5: %i\n", (int)end5);
    kprintf("final: %i\n\n",  (int)(end2 + end3 + end4 + end5));
  }
}

#ifdef __cplusplus
extern "C" {
#endif

void cdft(int, int, double *, int *, double *);

#ifdef __cplusplus
}
#endif

#ifndef NMAX
#define NMAXM (LOG2_FFT_LEN + 1)
#define NMAX (1 << NMAXM)
#define NMAXSQRT (1 << (NMAXM/2))
#endif

int reckon_start_old(void){
  i2c0_init((void*)i2c_reg, 1000000, METAL_I2C_MASTER);
  double w[NMAX * 5 / 4];
  int n = NMAX/2, ip[NMAXSQRT + 2];
  int m = NMAXM - 1;
  short fft[NMAX];
  short ffthw[NMAX];
  double fftd[NMAX];
  short* fftu;
  volatile uint32_t* fftdma = (uint32_t*)fftdma_reg;
  
  // Just test the heap
  fftu = (short*)malloc(NMAX*sizeof(short));
  free(fftu); 
  
  uint64_t start, endfft, endfftd, endffth;
  // Init the codec in 32-bit, 96KHz
  codec_init((void*)i2c_reg, CODEC_WORD_LENGTH_32B, CODEC_FORMAT_LEFT, CODEC_SAMPLING_ADC_96KHZ_DAC_96KHZ);
  while(1) {
    codec_sample_now((void*)codec_reg, l, r, n, CODEC_MASK_32B);
    // Convert to 16-bit
    memset(fft, 0, NMAX*sizeof(short));
    memset(ffthw, 0, NMAX*sizeof(short));
    for (int k = 0 ; k < n ; k++)
    {
      // The channel is the left one
      ffthw[k] = fft[k] = (short)(l[k] >> 16);
      if(fftdma) {
        fftdma[k] = (uint32_t)ffthw[k+n] << 16 | (uint32_t)ffthw[k];
      }
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
    cdft(2*n, 1, fftd, ip, w);
    endfftd = clkutils_read_mtime() - start;
    
    // Do hardware fft
    endffth = 0;
    if(fftdma_reg){
      _REG32(fft_reg, FFT_REG_CTRL) = FFT_CTRL_SYN_RST;
      
      start = clkutils_read_mtime();
      _REG32(fft_reg, FFT_REG_CTRL) = FFT_CTRL_START;
      while(!(_REG32(fft_reg, FFT_REG_STATUS) & FFT_STAT_READY));
      endffth = clkutils_read_mtime() - start;
    }
    else if(fft_reg) {
      start = clkutils_read_mtime();
      fft_hw(ffthw, ffthw+n, m, (void*)fft_reg);
      endffth = clkutils_read_mtime() - start;
    }
    
    kprintf("\r%i %i %i %i %i %i %i %i (%i %i %i %x %x)", 
        //l[0], l[NMAX/2/4*1], l[NMAX/2/4*2], l[NMAX/2/4*3], 
        (int)fft[0], (int)fft[NMAX/2/4*1], (int)fft[NMAX/2/4*2], (int)fft[NMAX/2/4*2], 
        (int)ffthw[0], (int)ffthw[NMAX/2/4*1], (int)ffthw[NMAX/2/4*2], (int)ffthw[NMAX/2/4*2], 
        (int)endfft, (int)endfftd, (int)endffth, (unsigned int)fft_reg, (unsigned int)fftdma_reg);
  }
}
