#ifndef H_SHADER_BUFFER
#define H_SHADER_BUFFER

#include <GL/gl.h>
#include <stdbool.h>

typedef struct _shader_channel shader_channel;
typedef struct _shader_uniform shader_uniform;
typedef struct _shader_context shader_context;
typedef struct _iMouse iMouse;

struct _shader_buffer {
  int width, height;
  unsigned int frame; // Frame counter
  double last_time;   // For calculating delta time
  char *shader_path;
  GLuint program;
  GLuint fbo;
  GLuint textures[2];  // Double-buffered textures
  int current_texture; // 0 or 1
  shader_channel *channel[4];
  shader_uniform *u;
  bool render_parity;
};

typedef struct _shader_buffer shader_buffer;

void free_shader_buffer(shader_buffer *buf);
bool init_shader_buffer(shader_buffer *buf, int width, int height,
                        char *shared_shader_path);
void render_shader_buffer(shader_context *ctx, shader_buffer *buf,
                          double current_time, iMouse *mouse);

#endif
