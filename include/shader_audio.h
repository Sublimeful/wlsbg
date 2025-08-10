#ifndef SHADER_AUDIO_H
#define SHADER_AUDIO_H

#include "miniaudio.h"
#include <GLES3/gl3.h>
#include <fftw3.h>
#include <pthread.h>
#include <stdbool.h>

#define AUDIO_BUFFER_SIZE 1024
#define AUDIO_TEXTURE_WIDTH 512
#define AUDIO_TEXTURE_HEIGHT 2

struct _shader_audio {
  char *path;
  ma_decoder decoder;
  ma_device device;
  bool is_playing;
  ma_format format;
  ma_uint32 channels;
  ma_uint32 sample_rate;
  double duration;
  struct timespec start_time;
  double seek_threshold;

  // Circular buffer
  float *circular_buffer;
  size_t circular_buffer_size;
  size_t write_pos;
  size_t read_pos;
  pthread_mutex_t buffer_mutex;

  // Processing buffers
  float *audio_buffer;
  float *waveform_data;
  float *frequency_data;
  float *frequency_smoothed;

  // FFT
  fftwf_complex *fft_in;
  fftwf_complex *fft_out;
  fftwf_plan fft_plan;

  // OpenGL texture
  GLuint texture_id;
};

typedef struct _shader_audio shader_audio;

shader_audio *shader_audio_create(char *path);
void shader_audio_update(shader_audio *audio, struct timespec start_time);
void shader_audio_destroy(shader_audio *audio);

#endif
