#include "shader_buffer.h"
#include "shader.h"
#include "shader_channel.h"
#include "shader_uniform.h"
#include <GLES3/gl3.h>
#include <stdlib.h>

void free_shader_buffer(shader_buffer *buf) {
  for (int i = 0; i < 4; i++) {
    if (buf->channel[i]) {
      free_shader_channel(buf->channel[i]);
    }
  }
  if (buf->program)
    glDeleteProgram(buf->program);
  glDeleteTextures(2, buf->textures);
  glDeleteFramebuffers(1, &buf->fbo);
  free(buf->u);
  free(buf->shader_path);
  free(buf);
}

bool init_shader_buffer(shader_buffer *buf, int width, int height) {
  buf->width = width;
  buf->height = height;
  buf->frame = 0;
  buf->last_time = 0;
  buf->render_parity = 0;

  if (!compile_and_link_program(&buf->program, buf->shader_path)) {
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
  set_uniform_locations(buf->program, buf->u);

  // Initialize buffer channels
  for (int i = 0; i < 4; i++) {
    if (buf->channel[i]) {
      init_channel_recursive(buf->channel[i], width, height);
    }
  }

  return true;
}

void render_shader_buffer(shader_context *ctx, shader_buffer *buf,
                          double current_time, iMouse *mouse) {
  // Change parity
  buf->render_parity = !buf->render_parity;

  // Recursively render inputs first
  for (int i = 0; i < 4; i++) {
    if (buf->channel[i] && buf->channel[i]->type == BUFFER &&
        buf->channel[i]->buf->render_parity != buf->render_parity) {
      render_shader_buffer(ctx, buf->channel[i]->buf, current_time, mouse);
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

  // Bind input textures
  for (int i = 0; i < 4; i++) {
    if (buf->channel[i]) {
      GLuint tex_id = get_channel_texture(buf->channel[i]);
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, tex_id);
      glUniform1i(buf->u->channel[i], i);
    }
  }

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
