#include "platform.h"
#include "kprintf.h"
#include "platform.h"
#include "i2c/i2c.h"
#include "clkutils.h"
#include "codec/codec.h"

#define CODEC_DATA(addr, data, ref) ref[0] = ((((addr) & 0x7F) << 1) | (((data) >> 8) & 0x1)); ref[1] = ((data) & 0xFF);
#define CODEC_ADDR 0x1A
#define CODEC_REG(i) _REG32(codec, i)

//based on SSM2603 datasheet
void codec_init(void* i2c, uint8_t word, uint8_t format, uint8_t sampling) {
  unsigned char buf[2];
  
  // Do a software reset
  CODEC_DATA(0x0F, 0, buf) 
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // Enable all power, except OUT (R6 D4)
  CODEC_DATA(0x06, 1 << 4, buf) 
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // Configuration
  // 1. Digital Audio IF. Data word length = "word" bits (R7 D3,D2). Format = "format" mode (R7, D1,D0)
  // 1b. Enable Master Mode in 1 also. (1 in R7, D6)
  CODEC_DATA(0x07, 1 << 6 | word << 2 | format << 0, buf) 
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // 2. Left ADC volume. No BOTH, No LinMute, Vol = 0dB (010111 in R0 D5-0)
  CODEC_DATA(0x00, 0 << 8 | 0 << 7 | 0x17, buf) 
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // 3. Right ADC volume. No BOTH, No LinMute, Vol = 0dB (010111 in R0 D5-0)
  CODEC_DATA(0x01, 0 << 8 | 0 << 7 | 0x17, buf)
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // 4. Enable USB mode (1 in R8 D0). Sampling will be "sampling". Dividers in zero 
  CODEC_DATA(0x08, sampling << 1 | 1, buf)
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // 5. Sample from mic (INSEL 1 R4 D2). No mute, no boost, no sidetone. no DACSEL, and no Bypass
  CODEC_DATA(0x04, 1 << 2, buf)
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // Wait 34ms
  uint64_t dest = metal_utime() + 34000;
  while(metal_utime() <= dest);
  // Active (R9 D0 in 1)
  CODEC_DATA(0x09, 1, buf)
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
  // Enable all power, 0
  CODEC_DATA(0x06, 0, buf) 
  i2c0_write(i2c, CODEC_ADDR, 2, buf, METAL_I2C_STOP_ENABLE);
}

void codec_sample_now(void* codec, uint32_t *destl, uint32_t *destr, uint32_t size, uint32_t mask) {
  CODEC_REG(CODEC_REG_CTRL) = CODEC_CTRL_CLR_AUD_IN; //Clear the FIFO
  while(size) {
    uint32_t timeout = metal_time() + 1;
    while(!(CODEC_REG(CODEC_REG_STATUS) & CODEC_STAT_AUD_IN_AVAIL)) {
      if(metal_time() > timeout) {
        kputs("WARNING: Timeout in codec sampling!\n");
        return;
      }
    }
    CODEC_REG(CODEC_REG_CTRL) = CODEC_CTRL_READ_AUD_IN;
    *destl = CODEC_REG(CODEC_REG_IN_L) & mask;
    *destr = CODEC_REG(CODEC_REG_IN_R) & mask;
    destl++;
    destr++;
    size--;
  }
}
