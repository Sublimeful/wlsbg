#ifndef SHADER_H
#define SHADER_H

#include <cairo.h>

// Opaque shader context
typedef struct shader_context shader_context;

shader_context *shader_context_create(const char *shader_path, int width,
                                      int height);
void shader_render(shader_context *ctx, cairo_surface_t *input,
                   cairo_surface_t *output, float time, float mouse_x,
                   float mouse_y);
void shader_context_destroy(shader_context *ctx);

#endif
