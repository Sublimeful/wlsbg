#ifndef SHADER_CHANNEL_H
#define SHADER_CHANNEL_H

#include <GL/gl.h>
#include <stdbool.h>

typedef struct _resource_registry resource_registry;
typedef struct _shader_texture shader_texture;
typedef struct _shader_buffer shader_buffer;

enum _shader_channel_type { NONE, TEXTURE, BUFFER };
typedef enum _shader_channel_type shader_channel_type;

struct _shader_channel {
  union {
    shader_texture *tex;
    shader_buffer *buf;
  };
  shader_channel_type type;
  bool initialized;
};

typedef struct _shader_channel shader_channel;

shader_channel *parse_channel_input(const char *input,
                                    resource_registry **registry_pointer);
void free_shader_channel(shader_channel *channel);
GLuint get_channel_texture(shader_channel *channel);
void init_channel_recursive(shader_channel *channel, int width, int height,
                            char *shared_shader_path);

#endif
