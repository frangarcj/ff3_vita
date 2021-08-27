#ifndef __CONFIG_H__
#define __CONFIG_H__

#define LOAD_ADDRESS 0x98000000

#define MEMORY_NEWLIB_MB 280
#define MEMORY_VITAGL_THRESHOLD_MB 8
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLES_PER_BUF 8192

#define DATA_PATH "ux0:data/ff5"
#define SO_PATH DATA_PATH "/" "libff5lib.so"
#define CONFIG_FILE_PATH DATA_PATH "/" "options.cfg"

#define DEF_SCREEN_W 960
#define DEF_SCREEN_H 544

#define DEBUG 1

typedef struct {
  int res;
  int bilinear;
  int lang;
  int msaa;
  int postfx;
} config_opts;
extern config_opts options;

#endif
