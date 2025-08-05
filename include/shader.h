#ifndef SHADER_H
#define SHADER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <stdbool.h>
#include <wayland-client-protocol.h>

typedef struct _shader_buffer shader_buffer;
typedef struct _iMouse iMouse;

struct _shader_context {
  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLConfig egl_config;
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;

  GLuint vao, vbo;

  shader_buffer *buf;

  bool initialized;
};

typedef struct _shader_context shader_context;

shader_context *shader_create(struct wl_display *display,
                              struct wl_surface *surface, char *shader_path,
                              char *shared_shader_path, int width, int height,
                              char *channel_input[10]);
void shader_render(shader_context *ctx, double current_time, iMouse *mouse);
void shader_resize(shader_context *ctx, int width, int height);
void shader_destroy(shader_context *ctx);

GLuint compile_shader(GLenum type, const char *source);
bool compile_and_link_program(GLuint *program, char *shader_path,
                              char *shared_shader_path);

#endif
