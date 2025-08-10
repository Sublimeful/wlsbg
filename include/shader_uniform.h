#ifndef H_SHADER_UNIFORM
#define H_SHADER_UNIFORM

#include <GL/gl.h>
#include <time.h>

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
  GLint resolution;      // Uniform location for iResolution
  GLint time;            // Uniform location for iTime
  GLint time_delta;      // Uniform location for iTimeDelta
  GLint mouse;           // Uniform location for iMouse
  GLint mouse_pos;       // Uniform location for iMousePos
  GLint frame;           // Uniform location for iFrame
  GLint frame_rate;      // Uniform location for iFrameRate
  GLint date;            // Uniform location for iDate
  GLint channel[10];     // Uniform locations for iChannel
  GLint channel_res[10]; // Uniform locations for iChannelResolution
  GLint channel_dur[10]; // Uniform locations for iChannelDuration
};

typedef struct _shader_uniform shader_uniform;

void set_uniform_locations(GLuint program, shader_uniform *u);
void set_uniforms(shader_buffer *buf, struct timespec start_time,
                  iMouse *mouse);

#endif
