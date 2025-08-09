#ifndef SHADER_VIDEO_H
#define SHADER_VIDEO_H

#include <GL/gl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>

struct _shader_video {
  mpv_handle *mpv;
  mpv_render_context *mpv_gl;
  GLuint tex_id;
  GLuint fbo;
  int width;
  int height;
  int wakeup_fd;
  double last_seek_time;
  double video_duration;
};

typedef struct _shader_video shader_video;

shader_video *shader_video_create(const char *path);
void shader_video_update(shader_video *vid, double current_time);
void shader_video_render(shader_video *vid);
void shader_video_destroy(shader_video *vid);

#endif
