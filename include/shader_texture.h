#ifndef SHADER_TEXTURE_H
#define SHADER_TEXTURE_H

#include <GL/gl.h>
#include <stdbool.h>

struct _shader_texture {
  char *path;
  GLuint tex_id;
  int width, height;
};

typedef struct _shader_texture shader_texture;

bool load_shader_texture(shader_texture *tex);

#endif
