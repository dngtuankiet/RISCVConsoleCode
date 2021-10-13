#ifndef _CODEC_H
#define _CODEC_H

#include <inttypes.h>

#define CODEC_WORD_LENGTH_16B 0 // 00
#define CODEC_WORD_LENGTH_20B 1 // 01
#define CODEC_WORD_LENGTH_24B 2 // 10
#define CODEC_WORD_LENGTH_32B 3 // 11

#define CODEC_FORMAT_RIGHT 0 // 00
#define CODEC_FORMAT_LEFT 1 // 01
#define CODEC_FORMAT_I2S 2 // 10
// #define CODEC_FORMAT_DSP 3 // DSP mode not supported

// Samplings supported in USB mode 12MHz
#define CODEC_SAMPLING_ADC_8KHZ_DAC_8KHZ (0x3 << 1 | 0) // 0011 0
#define CODEC_SAMPLING_ADC_8KHZ_DAC_48KHZ (0x2 << 1 | 0) // 0010 0
#define CODEC_SAMPLING_ADC_8p0214KHZ_DAC_8p0214KHZ (0xB << 1 | 1) // 1011 1
#define CODEC_SAMPLING_ADC_8p0214KHZ_DAC_44p118KHZ (0xA << 1 | 1) // 1010 1
#define CODEC_SAMPLING_ADC_11p0259KHZ_DAC_11p0259KHZ (0xC << 1 | 1) // 1100 1
#define CODEC_SAMPLING_ADC_12KHZ_DAC_12KHZ (0x8 << 1 | 0) // 1000 0
#define CODEC_SAMPLING_ADC_16KHZ_DAC_16KHZ (0xA << 1 | 0) // 1010 0
#define CODEC_SAMPLING_ADC_22p0588KHZ_DAC_22p0588KHZ (0xD << 1 | 1) // 1101 1
#define CODEC_SAMPLING_ADC_24KHZ_DAC_24KHZ (0xE << 1 | 0) // 1110 0
#define CODEC_SAMPLING_ADC_32KHZ_DAC_32KHZ (0x6 << 1 | 0) // 0110 0
#define CODEC_SAMPLING_ADC_44p118KHZ_DAC_8p0214KHZ (0x9 << 1 | 1) // 1001 1
#define CODEC_SAMPLING_ADC_44p118KHZ_DAC_44p118KHZ (0x8 << 1 | 1) // 1000 1
#define CODEC_SAMPLING_ADC_48KHZ_DAC_8KHZ (0x1 << 1 | 0) // 0001 0
#define CODEC_SAMPLING_ADC_48KHZ_DAC_48KHZ (0x0 << 1 | 0) // 0000 0
#define CODEC_SAMPLING_ADC_88p235KHZ_DAC_88p235KHZ (0xF << 1 | 1) // 1111 1
#define CODEC_SAMPLING_ADC_96KHZ_DAC_96KHZ (0x7 << 1 | 0) // 0111 0

#define CODEC_MASK_16B 0x0000FFFF
#define CODEC_MASK_20B 0x000FFFFF
#define CODEC_MASK_24B 0x00FFFFFF
#define CODEC_MASK_32B 0xFFFFFFFF

void codec_init(void* i2c, uint8_t word, uint8_t format, uint8_t sampling);
void codec_sample_now(void* codec, uint32_t *destl, uint32_t *destr, uint32_t size, uint32_t mask);
void codec_record_demo();

#endif // _CODEC_H
