#include "shader_uniform.h"
#include "shader_buffer.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <time.h>

void set_uniform_locations(GLuint program, shader_uniform *u) {
  // Get uniform locations
  u->resolution = glGetUniformLocation(program, "iResolution");
  u->time = glGetUniformLocation(program, "iTime");
  u->time_delta = glGetUniformLocation(program, "iTimeDelta");
  u->frame = glGetUniformLocation(program, "iFrame");
  u->frame_rate = glGetUniformLocation(program, "iFrameRate");
  u->mouse = glGetUniformLocation(program, "iMouse");
  u->mouse_pos = glGetUniformLocation(program, "iMousePos");
  u->date = glGetUniformLocation(program, "iDate");

  // Get uniform locations for iChannel0-9
  for (int i = 0; i < 10; i++) {
    char name[16];
    snprintf(name, sizeof(name), "iChannel%d", i);
    u->channel[i] = glGetUniformLocation(program, name);
  }

  // Get channel resolution uniforms
  for (int i = 0; i < 10; i++) {
    char name[32];
    snprintf(name, sizeof(name), "iChannelResolution[%d]", i);
    u->channel_res[i] = glGetUniformLocation(program, name);
  }
}

void set_uniforms(shader_buffer *buf, double current_time, iMouse *mouse) {
  // Calculate delta time and fps
  double delta = (buf->frame == 0) ? 0 : (current_time - buf->last_time);
  float fps = (delta > 0) ? (1.0f / delta) : 0;

  if (buf->u->resolution >= 0)
    glUniform3f(buf->u->resolution, (float)buf->width, (float)buf->height,
                (float)buf->width / buf->height);
  if (buf->u->time >= 0)
    glUniform1f(buf->u->time, (float)current_time);
  if (buf->u->time_delta >= 0)
    glUniform1f(buf->u->time_delta, (float)delta);
  if (mouse) {
    if (buf->u->mouse >= 0)
      glUniform4f(buf->u->mouse, mouse->x, mouse->y, mouse->z, mouse->w);
    if (buf->u->mouse_pos >= 0)
      glUniform2f(buf->u->mouse_pos, mouse->real_x, mouse->real_y);
  }
  if (buf->u->frame >= 0)
    glUniform1i(buf->u->frame, buf->frame);
  if (buf->u->frame_rate >= 0)
    glUniform1f(buf->u->frame_rate, fps);
  if (buf->u->date >= 0) {
    // Get current date/time
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    float seconds = tm->tm_sec + tm->tm_min * 60 + tm->tm_hour * 3600;
    glUniform4f(buf->u->date, (float)(tm->tm_year + 1900), (float)tm->tm_mon,
                (float)tm->tm_mday, seconds);
  }
}
