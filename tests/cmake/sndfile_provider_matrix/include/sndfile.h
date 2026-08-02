#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t sf_count_t;
typedef struct SNDFILE_tag SNDFILE;

typedef struct SF_INFO {
  sf_count_t frames;
  int samplerate;
  int channels;
  int format;
  int sections;
  int seekable;
} SF_INFO;

#define SFM_READ 0x10
#define SFM_WRITE 0x20
#define SF_FORMAT_WAV 0x010000
#define SF_FORMAT_AIFF 0x020000
#define SF_FORMAT_FLAC 0x170000
#define SF_FORMAT_PCM_16 0x0002
#define SF_FORMAT_PCM_24 0x0003
#define SF_FORMAT_PCM_32 0x0004
#define SF_FORMAT_FLOAT 0x0006
#define SF_FORMAT_TYPEMASK 0x0FFF0000
#define SF_FORMAT_SUBMASK 0x0000FFFF
#define SFC_SET_CLIPPING 0x10
#define SFC_SET_ADD_PEAK_CHUNK 0x1070
#define SF_TRUE 1
#define SF_FALSE 0

SNDFILE* sf_open(const char* path, int mode, SF_INFO* info);
int sf_close(SNDFILE* file);
const char* sf_strerror(const SNDFILE* file);
sf_count_t sf_readf_float(SNDFILE* file, float* buffer, sf_count_t frames);
sf_count_t sf_writef_float(SNDFILE* file, const float* buffer, sf_count_t frames);
sf_count_t sf_seek(SNDFILE* file, sf_count_t frames, int whence);
int sf_format_check(const SF_INFO* info);
int sf_command(SNDFILE* file, int command, void* data, int datasize);

#ifdef __cplusplus
}
#endif
