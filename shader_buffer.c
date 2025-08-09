#include "shader_buffer.h"
#include "shader.h"
#include "shader_audio.h"
#include "shader_channel.h"
#include "shader_uniform.h"
#include "shader_video.h"
#include <GLES3/gl3.h>
#include <stdlib.h>
#include <string.h>

void free_shader_buffer(shader_buffer *buf) {
  if (!buf)
    return;

  if (buf->program)
    glDeleteProgram(buf->program);
  if (buf->fbo)
    glDeleteFramebuffers(1, &buf->fbo);
  if (buf->textures[0] || buf->textures[1])
    glDeleteTextures(2, buf->textures);
  free(buf->u);
  free(buf->shader_path);
  free(buf);
}

bool init_shader_buffer(shader_buffer *buf, int width, int height,
                        char *shared_shader_path) {
  if (!buf)
    return false;

  buf->width = width;
  buf->height = height;
  buf->frame = 0;
  buf->last_time = 0;
  buf->render_parity = 0;

  if (!compile_and_link_program(&buf->program, buf->shader_path,
                                shared_shader_path)) {
    return false;
  }

  // Create FBO and textures
  glGenFramebuffers(1, &buf->fbo);
  glGenTextures(2, buf->textures);
  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, buf->textures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                 GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  // Attach first texture to FBO
  glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         buf->textures[0], 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  buf->current_texture = 0;

  // Initialize buffer uniforms
  buf->u = malloc(sizeof(shader_uniform));
  if (!buf->u) {
    return false;
  }
  set_uniform_locations(buf->program, buf->u);

  // Initialize buffer channels
  for (int i = 0; i < 10; i++) {
    if (!buf->channel[i])
      continue;
    if (!init_channel_recursive(buf->channel[i], width, height,
                                shared_shader_path)) {
      return false;
    }
  }

  return true;
}

void render_shader_buffer(shader_context *ctx, shader_buffer *buf,
                          double current_time, iMouse *mouse) {
  if (!ctx || !buf)
    return;

  // Change parity
  buf->render_parity = !buf->render_parity;

  // Recursively render buffer inputs
  for (int i = 0; i < 10; i++) {
    if (!buf->channel[i])
      continue;
    switch (buf->channel[i]->type) {
    case BUFFER:
      if (buf->channel[i]->buf->render_parity == buf->render_parity)
        break;
      render_shader_buffer(ctx, buf->channel[i]->buf, current_time, mouse);
      break;
    case VIDEO:
      shader_video_update(buf->channel[i]->vid, current_time);
      shader_video_render(buf->channel[i]->vid);
      break;
    case AUDIO:
      shader_audio_update(buf->channel[i]->aud, current_time);
      break;
    default:
      break;
    }
  }

  // Setup FBO
  int next_tex = 1 - buf->current_texture;
  glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         buf->textures[next_tex], 0);

  // Render buffer shader
  glUseProgram(buf->program);
  glViewport(0, 0, buf->width, buf->height);

  // Set uniforms
  set_uniforms(buf, current_time, mouse);

  // Draw
  glBindVertexArray(ctx->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Cleanup
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Swap buffers
  buf->current_texture = next_tex;

  // Update state for next frame
  buf->last_time = current_time;
  buf->frame++;
}
