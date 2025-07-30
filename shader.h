#ifndef SHADER_H
#define SHADER_H

#include <EGL/egl.h>
#include <GL/gl.h>
#include <cairo.h>
#include <stdbool.h>

typedef struct shader_context {
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
  GLuint program;
  GLuint texture;
  GLuint vao;
  GLuint vbo;
  GLuint ubo;
  int width;
  int height;
} shader_context;

shader_context *shader_context_create(const char *shader_path, int width,
                                      int height);
void shader_render(shader_context *ctx, cairo_surface_t *input,
                   cairo_surface_t *output, double time, float mouse_x,
                   float mouse_y, float down_x, float down_y, float click_x,
                   float click_y, bool is_down, bool is_clicked);
void shader_context_destroy(shader_context *ctx);

#endif
