#include "platform.h"
#include "kprintf.h"
#include "platform.h"
#include "i2c/i2c.h"
#include "clkutils.h"
#include "main.h"
#include "codec/codec.h"

void codec_record_demo()
{
  i2c0_init((void*)i2c_reg, 1000000, METAL_I2C_MASTER);
  int32_t l[128];
  int32_t r[128];
  kputs("Sample recording demo running...\n");
  codec_init((void*)i2c_reg, CODEC_WORD_LENGTH_32B, CODEC_FORMAT_LEFT, CODEC_SAMPLING_ADC_96KHZ_DAC_96KHZ);
  while(1) {
    codec_sample_now((void*)codec_reg, l, r, 128, CODEC_MASK_32B);
    kprintf("\r%i %i %i %i", l[0], l[32], l[64], l[96]);
    for(int i = 0; i < 128; i++) {
      // Sawtooth
      l[i] = i << 20;
      // Inverted Sawtooth
      r[i] = (127-i) << 20;
    }
    codec_play_now((void*)codec_reg, l, r, 128);
  }
}
