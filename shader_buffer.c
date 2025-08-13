#include "shader_buffer.h"
#include "shader.h"
#include "shader_audio.h"
#include "shader_channel.h"
#include "shader_texture.h"
#include "shader_uniform.h"
#include "shader_video.h"
#include "util.h"
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
                          struct timespec start_time, iMouse *mouse) {
  if (!ctx || !buf)
    return;

  // Flip parity so we don't re-render same buffer multiple times this frame
  buf->render_parity = !buf->render_parity;

  // First: recursively render any buffer inputs / update media
  for (int i = 0; i < 10; i++) {
    if (!buf->channel[i])
      continue;
    switch (buf->channel[i]->type) {
    case BUFFER:
      // Avoid re-rendering buffers that were already rendered this frame
      if (buf->channel[i]->buf->render_parity == buf->render_parity)
        break;
      render_shader_buffer(ctx, buf->channel[i]->buf, start_time, mouse);
      break;
    case VIDEO:
      shader_video_update(buf->channel[i]->vid, start_time);
      shader_video_render(buf->channel[i]->vid);
      break;
    case AUDIO:
      shader_audio_update(buf->channel[i]->aud, start_time);
      break;
    default:
      break;
    }
  }

  // Ping-pong: we'll read from prev_tex and write to next_tex
  int prev_tex = buf->current_texture;
  int next_tex = 1 - buf->current_texture;

  // Bind FBO and attach the texture we will render into (next_tex)
  glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         buf->textures[next_tex], 0);

  // Use program and viewport
  glUseProgram(buf->program);
  glViewport(0, 0, buf->width, buf->height);

  // Bind channels as iChannel0..iChannel9 (texture units 0..9)
  for (int i = 0; i < 10; i++) {
    if (!buf->channel[i])
      continue;

    GLuint tex_id = 0;

    switch (buf->channel[i]->type) {
    case BUFFER:
      if (buf->channel[i]->buf == buf) {
        // Self-feedback: bind last frame's texture
        tex_id = buf->textures[prev_tex];
      } else {
        // Other buffer: bind its most-recent texture
        shader_buffer *other = buf->channel[i]->buf;
        tex_id = other->textures[other->current_texture];
      }
      break;
    case TEXTURE:
      tex_id = buf->channel[i]->tex->tex_id;
      break;
    case VIDEO:
      tex_id = buf->channel[i]->vid->tex_id;
      break;
    case AUDIO:
      tex_id = buf->channel[i]->aud->tex_id;
      break;
    default:
      continue;
    }

    // Ignore invalid texture ids
    if (!tex_id)
      continue;

    // Bind to texture unit i
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    // Set the shader sampler uniform (iChannelN)
    if (buf->u->channel[i] >= 0) {
      glUniform1i(buf->u->channel[i], i);
    }
  }

  // Also set any other uniforms (iTime, iResolution, mouse, frame, etc.)
  // set_uniforms should set uniforms that aren't channel samplers.
  set_uniforms(buf, start_time, mouse);

  // Draw
  glBindVertexArray(ctx->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Unbind FBO so subsequent rendering goes to default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Swap: the texture we just rendered into becomes the current output
  buf->current_texture = next_tex;

  // Update state for next frame
  buf->last_time = time_elapsed(start_time);
  buf->frame++;
}
