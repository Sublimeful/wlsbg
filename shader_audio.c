#define MINIAUDIO_IMPLEMENTATION

#include "shader_audio.h"
#include "util.h"
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979323846f

void audio_data_callback(ma_device *device, void *output, const void *input,
                         ma_uint32 frame_count) {
  shader_audio *audio = (shader_audio *)device->pUserData;
  if (!audio)
    return;

  (void)input; // input is not used in playback mode

  ma_uint64 frames_read;
  ma_uint64 cursor;

  ma_result result;
  result = ma_decoder_read_pcm_frames(&audio->decoder, audio->audio_buffer,
                                      frame_count, &frames_read);
  // When audio playback finishes, loop
  if (result != MA_SUCCESS) {
    ma_decoder_seek_to_pcm_frame(&audio->decoder, 0);
    ma_decoder_read_pcm_frames(&audio->decoder, audio->audio_buffer,
                               frame_count, &frames_read);
  }

  // Check if we need to sync up
  result = ma_decoder_get_cursor_in_pcm_frames(&audio->decoder, &cursor);
  if (result == MA_SUCCESS) {
    double current_pos = (double)cursor / audio->sample_rate;
    double elapsed_time = time_elapsed(audio->start_time);
    double target_time = fmod(elapsed_time, audio->duration);

    double time_diff = fabs(target_time - current_pos);
    if (time_diff > audio->seek_threshold) {
      double target_frames = target_time * audio->sample_rate;
      ma_decoder_seek_to_pcm_frame(&audio->decoder, target_frames);
      ma_decoder_read_pcm_frames(&audio->decoder, audio->audio_buffer,
                                 frame_count, &frames_read);
    }
  }

  pthread_mutex_lock(&audio->buffer_mutex);

  for (ma_uint32 i = 0; i < frames_read * audio->channels; i++) {
    audio->circular_buffer[audio->write_pos] = audio->audio_buffer[i];
    audio->write_pos = (audio->write_pos + 1) % audio->circular_buffer_size;
  }

  pthread_mutex_unlock(&audio->buffer_mutex);

  float *float_output = (float *)output;

  // Write to output to create sound
  for (ma_uint32 i = 0; i < frames_read * audio->channels; i++) {
    float_output[i] = audio->audio_buffer[i];
  }

  // Fill remainder with silence
  for (ma_uint32 i = frames_read * audio->channels;
       i < frame_count * audio->channels; i++) {
    float_output[i] = 0.0f;
  }
}

shader_audio *shader_audio_create(char *path) {
  shader_audio *audio = calloc(1, sizeof(shader_audio));
  if (!audio)
    return NULL;

  audio->path = path;
  if (!audio->path) {
    free(audio);
    return NULL;
  }

  // Initialize decoder
  ma_result result = ma_decoder_init_file(path, NULL, &audio->decoder);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "Failed to initialize the audio decoder: %s\n",
            ma_result_description(result));
    free(audio);
    return NULL;
  }

  // Get audio format
  ma_format format;
  ma_uint32 channels;
  ma_uint32 sampleRate;
  result = ma_decoder_get_data_format(&audio->decoder, &format, &channels,
                                      &sampleRate, NULL, 0);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "Failed to read the audio data format: %s\n",
            ma_result_description(result));
    ma_decoder_uninit(&audio->decoder);
    free(audio);
    return NULL;
  }
  audio->format = format;
  audio->channels = channels;
  audio->sample_rate = sampleRate;

  // Get and set the duration of the audio
  ma_uint64 length;
  result = ma_decoder_get_length_in_pcm_frames(&audio->decoder, &length);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "Failed to get the length of the audio: %s\n",
            ma_result_description(result));
    ma_decoder_uninit(&audio->decoder);
    free(audio);
    return NULL;
  }
  audio->duration = (double)length / audio->sample_rate;

  // Setup circular buffer (2 seconds of audio)
  audio->circular_buffer_size = sampleRate * channels * 2;
  audio->circular_buffer = calloc(audio->circular_buffer_size, sizeof(float));
  if (!audio->circular_buffer) {
    ma_decoder_uninit(&audio->decoder);
    free(audio);
    return NULL;
  }

  pthread_mutex_init(&audio->buffer_mutex, NULL);

  // Initialize FFT
  audio->fft_in = fftwf_malloc(sizeof(fftwf_complex) * AUDIO_BUFFER_SIZE);
  audio->fft_out = fftwf_malloc(sizeof(fftwf_complex) * AUDIO_BUFFER_SIZE);
  audio->fft_plan =
      fftwf_plan_dft_1d(AUDIO_BUFFER_SIZE, audio->fft_in, audio->fft_out,
                        FFTW_FORWARD, FFTW_ESTIMATE);

  // Initialize processing buffers
  audio->audio_buffer = calloc(AUDIO_BUFFER_SIZE * 2, sizeof(float));
  audio->frequency_data = calloc(AUDIO_TEXTURE_WIDTH, sizeof(float));
  audio->frequency_smoothed = calloc(AUDIO_TEXTURE_WIDTH, sizeof(float));
  audio->waveform_data = calloc(AUDIO_TEXTURE_WIDTH, sizeof(float));

  // Create OpenGL texture
  glGenTextures(1, &audio->texture_id);
  glBindTexture(GL_TEXTURE_2D, audio->texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, AUDIO_TEXTURE_WIDTH,
               AUDIO_TEXTURE_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Configure audio device
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = channels;
  config.sampleRate = sampleRate;
  config.dataCallback = audio_data_callback;
  config.pUserData = audio;

  if (ma_device_init(NULL, &config, &audio->device) != MA_SUCCESS) {
    fprintf(stderr, "Failed to initialize audio device\n");
    shader_audio_destroy(audio);
    return NULL;
  }

  // Initialize audio members
  audio->start_time = current_time();
  audio->seek_threshold = 0.5; // Only seek if desynced by more than 500ms
  audio->is_playing = true;

  // Start playback
  if (ma_device_start(&audio->device) != MA_SUCCESS) {
    fprintf(stderr, "Failed to start audio device\n");
    shader_audio_destroy(audio);
    return NULL;
  }

  return audio;
}

void shader_audio_update(shader_audio *audio, struct timespec start_time) {
  if (!audio || !audio->is_playing)
    return;

  audio->start_time = start_time;

  pthread_mutex_lock(&audio->buffer_mutex);

  // Calculate available samples
  size_t available;
  if (audio->write_pos >= audio->read_pos) {
    available = audio->write_pos - audio->read_pos;
  } else {
    available =
        (audio->circular_buffer_size - audio->read_pos) + audio->write_pos;
  }

  // Need at least AUDIO_BUFFER_SIZE * channels samples
  if (available < AUDIO_BUFFER_SIZE * audio->channels) {
    pthread_mutex_unlock(&audio->buffer_mutex);
    return;
  }

  // Copy samples to processing buffer
  for (ma_uint32 i = 0; i < AUDIO_BUFFER_SIZE * audio->channels; i++) {
    audio->audio_buffer[i] = audio->circular_buffer[audio->read_pos];
    audio->read_pos = (audio->read_pos + 1) % audio->circular_buffer_size;
  }

  pthread_mutex_unlock(&audio->buffer_mutex);

  // Process waveform data
  for (int i = 0; i < AUDIO_TEXTURE_WIDTH; i++) {
    int idx = (i * AUDIO_BUFFER_SIZE) / AUDIO_TEXTURE_WIDTH;
    if (audio->channels == 2) {
      audio->waveform_data[i] =
          (audio->audio_buffer[idx * 2] + audio->audio_buffer[idx * 2 + 1]) *
          0.5f;
    } else {
      audio->waveform_data[i] = audio->audio_buffer[idx];
    }
  }

  // Prepare FFT input (convert to mono)
  for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    if (audio->channels == 2) {
      float mono =
          (audio->audio_buffer[i * 2] + audio->audio_buffer[i * 2 + 1]) * 0.5f;
      audio->fft_in[i][0] = mono;
      audio->fft_in[i][1] = 0;
    } else {
      audio->fft_in[i][0] = audio->audio_buffer[i];
      audio->fft_in[i][1] = 0;
    }
  }

  // Execute FFT
  fftwf_execute(audio->fft_plan);

  // Process frequency data
  for (int i = 0; i < AUDIO_TEXTURE_WIDTH; i++) {
    int bin = (i * (AUDIO_BUFFER_SIZE / 2)) / AUDIO_TEXTURE_WIDTH;
    float real = audio->fft_out[bin][0];
    float imag = audio->fft_out[bin][1];
    float magnitude =
        sqrtf(real * real + imag * imag) / (AUDIO_BUFFER_SIZE / 2.0);

    // Apply logarithmic scale
    magnitude = logf(1.0f + magnitude * 10.0f);

    // Smooth with exponential moving average
    audio->frequency_data[i] =
        audio->frequency_smoothed[i] * 0.9f + magnitude * 0.1f;
    audio->frequency_smoothed[i] = audio->frequency_data[i];
  }

  // Update texture
  float texture_data[AUDIO_TEXTURE_WIDTH * AUDIO_TEXTURE_HEIGHT];

  // Waveform (first row)
  for (int i = 0; i < AUDIO_TEXTURE_WIDTH; i++) {
    texture_data[i] = audio->waveform_data[i];
  }

  // Spectrum (second row)
  int offset = AUDIO_TEXTURE_WIDTH;
  for (int i = 0; i < AUDIO_TEXTURE_WIDTH; i++) {
    texture_data[offset + i] = audio->frequency_data[i];
  }

  glBindTexture(GL_TEXTURE_2D, audio->texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AUDIO_TEXTURE_WIDTH,
                  AUDIO_TEXTURE_HEIGHT, GL_RED, GL_FLOAT, texture_data);
}

void shader_audio_destroy(shader_audio *audio) {
  if (!audio)
    return;

  free(audio->path);
  audio->path = NULL;

  if (audio->is_playing) {
    ma_device_stop(&audio->device);
    ma_device_uninit(&audio->device);
  }

  ma_decoder_uninit(&audio->decoder);

  if (audio->texture_id) {
    glDeleteTextures(1, &audio->texture_id);
  }

  fftwf_destroy_plan(audio->fft_plan);
  fftwf_free(audio->fft_in);
  fftwf_free(audio->fft_out);

  pthread_mutex_destroy(&audio->buffer_mutex);

  free(audio->circular_buffer);
  free(audio->audio_buffer);
  free(audio->waveform_data);
  free(audio->frequency_data);
  free(audio->frequency_smoothed);
  free(audio);
}
