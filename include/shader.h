#ifndef SHADER_H
#define SHADER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>
#include "shader_channel.h"

typedef struct {
  float real_x;
  float real_y;
  float x;
  float y;
  float z;
  float w;
} iMouse;


typedef struct {
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLConfig egl_config;
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;

  GLuint program;
  GLuint vao, vbo;

  shader_channel* channel[4]; // A, B, C, D

  // Uniforms
  GLint u_resolution;
  GLint u_time;       // Time in seconds
  GLint u_time_delta; // Uniform location for iTimeDelta
  GLint u_mouse;
  GLint u_mouse_pos;
  GLint u_frame;          // Uniform location for iFrame
  GLint u_frame_rate;     // Uniform location for iFrameRate
  GLint u_date;           // Uniform location for iDate
  GLint u_channel[4];     // Uniform locations for iChannel0-iChannel3
  GLint u_channel_res[4]; // Uniform locations for iChannelResolution

  unsigned int frame; // Frame counter
  double last_time;   // For calculating delta time
  int width, height;  // Width and Height of the shader
  bool initialized;
} shader_context;

shader_context *shader_create(struct wl_display *display,
                              struct wl_surface *surface,
                              const char *shader_path, int width, int height,
                              char *channel_input[4]);
void shader_render(shader_context *ctx, double current_time, iMouse *mouse,
                   unsigned char *image_data, int image_width,
                   int image_height);
void shader_resize(shader_context *ctx, int width, int height);
void shader_destroy(shader_context *ctx);

#endif
