#ifndef SHADER_H
#define SHADER_H

#include <cairo.h>
#include <stdbool.h>

// Opaque shader context
typedef struct shader_context shader_context;

shader_context *shader_context_create(const char *shader_path, int width,
                                      int height);
void shader_render(shader_context *ctx, cairo_surface_t *input,
                   cairo_surface_t *output, float time, float mouse_x,
                   float mouse_y, bool is_down, float down_x, float down_y,
                   float click_x, float click_y);
void shader_context_destroy(shader_context *ctx);

#endif
