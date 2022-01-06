#include <platform.h>
#define FFT_REG(i) _REG32(dev, i)
#define FFT_REG16(i) _REG16(dev, i)
#define max(a, b) ((a) > (b) ? (a) : (b))

int fix_fft(short fr[], short fi[], short m, void* dev)
{
  // Trigger the reset once
  FFT_REG(FFT_REG_CTRL) = FFT_CTRL_SYN_RST;
  int n = max(1 << m, 1 << LOG2_FFT_LEN);
  for(int i = 0; i < n; i++) {
	  FFT_REG(FFT_REG_ADDR_IN) = i;
	  FFT_REG(FFT_REG_DATA_IN) =  (uint32_t)fi[i] << 16 | (uint32_t)fr[i];
    FFT_REG(FFT_REG_CTRL) = FFT_CTRL_WR_IN;
  }
  FFT_REG(FFT_REG_CTRL) = FFT_CTRL_START;
  while(!(FFT_REG(FFT_REG_STATUS) & FFT_STAT_READY));
  for(int i = 0; i < n; i++) {
	  FFT_REG(FFT_REG_ADDR_OUT) = i;
	  fi[i] = FFT_REG16(FFT_REG_DATA_OUT + 2);
	  fr[i] = FFT_REG16(FFT_REG_DATA_OUT);
  }
}

