#ifndef SHADER_VIDEO_H
#define SHADER_VIDEO_H

#include <GL/gl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <stdbool.h>
#include <time.h>

struct _shader_video {
  char *path;
  mpv_handle *mpv;
  mpv_render_context *mpv_gl;
  GLuint tex_id;
  GLuint fbo;
  int width;
  int height;
  int wakeup_fd;
  struct timespec last_seek_time;
  double seek_threshold;
  double seek_cooldown;
  double speed_adjustment_threshold;
  double duration;
  bool fbo_configured;
  bool playing;
  bool seeking;
};

typedef struct _shader_video shader_video;

shader_video *shader_video_create(char *path);
void shader_video_update(shader_video *vid, struct timespec start_time);
void shader_video_render(shader_video *vid);
void shader_video_destroy(shader_video *vid);

#endif
