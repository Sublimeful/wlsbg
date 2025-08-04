#ifndef SHADER_CHANNEL_H
#define SHADER_CHANNEL_H

#include <GL/gl.h>

typedef struct {
  char *path;
  unsigned char *data;
  int width, height;
} shader_texture;

typedef enum { NONE, TEXTURE, BUFFER } shader_channel_type;

typedef struct _shader_buffer shader_buffer;
typedef struct _shader_channel shader_channel;

typedef struct _shader_buffer {
  char *shader_path;
  GLuint fbo;
  GLuint textures[2];  // Double-buffered textures
  int current_texture; // 0 or 1
  shader_channel *channel[4];
} shader_buffer;

typedef struct _shader_channel {
  union {
    shader_texture *tex;
    shader_buffer *buf;
  };
  shader_channel_type type;
} shader_channel;

shader_channel *parse_channel_input(const char *input);
void free_shader_channel(shader_channel *channel);

#endif
