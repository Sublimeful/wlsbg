#ifndef H_SHADER_UNIFORM
#define H_SHADER_UNIFORM

#include <GL/gl.h>

typedef struct _shader_buffer shader_buffer;

struct _iMouse {
  float real_x;
  float real_y;
  float x;
  float y;
  float z;
  float w;
};

typedef struct _iMouse iMouse;

struct _shader_uniform {
  GLint resolution;
  GLint time;       // Time in seconds
  GLint time_delta; // Uniform location for iTimeDelta
  GLint mouse;
  GLint mouse_pos;
  GLint frame;          // Uniform location for iFrame
  GLint frame_rate;     // Uniform location for iFrameRate
  GLint date;           // Uniform location for iDate
  GLint channel[4];     // Uniform locations for iChannel0-iChannel3
  GLint channel_res[4]; // Uniform locations for iChannelResolution
};

typedef struct _shader_uniform shader_uniform;

void set_uniform_locations(GLuint program, shader_uniform *u);
void set_uniforms(shader_buffer *buf, double current_time, iMouse *mouse);

#endif
