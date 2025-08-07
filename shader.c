#define STB_IMAGE_IMPLEMENTATION

#include "shader.h"
#include "resource_registry.h"
#include "shader_buffer.h"
#include "shader_channel.h"
#include "shader_texture.h"
#include "shader_uniform.h"
#include "stb_image.h"
#include "util.h"
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-egl.h>

static const char *VERTEX_SHADER_SOURCE =
    "#version 320 es\n"
    "layout(location = 0) in vec2 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char *FRAGMENT_SHADER_TEMPLATE =
    "#version 320 es\n"
    "precision highp float;\n"
    "uniform vec3 iResolution;\n"
    "uniform float iTime;\n"
    "uniform float iTimeDelta;\n"
    "uniform int iFrame;\n"
    "uniform float iFrameRate;\n"
    "uniform vec4 iMouse;\n"
    "uniform vec2 iMousePos;\n"
    "uniform vec4 iDate;\n"
    "uniform sampler2D iChannel0;\n"
    "uniform sampler2D iChannel1;\n"
    "uniform sampler2D iChannel2;\n"
    "uniform sampler2D iChannel3;\n"
    "uniform sampler2D iChannel4;\n"
    "uniform sampler2D iChannel5;\n"
    "uniform sampler2D iChannel6;\n"
    "uniform sampler2D iChannel7;\n"
    "uniform sampler2D iChannel8;\n"
    "uniform sampler2D iChannel9;\n"
    "uniform vec3 iChannelResolution[10];\n"
    "out vec4 fragColor;\n"
    "%s\n"
    "%s\n"
    "void main() {\n"
    "    mainImage(fragColor, gl_FragCoord.xy);\n"
    "}\n";

// Simple vertex data - single triangle covering entire screen
static const float vertices[] = {
    -1.0f, -3.0f, // Bottom-left (extends beyond screen)
    -1.0f, 1.0f,  // Top-left
    3.0f,  1.0f   // Top-right (extends beyond screen)
};

GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  if (!shader)
    return 0;

  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    fprintf(stderr, "Shader compilation failed: %s\n", info_log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

bool compile_and_link_program(GLuint *program, char *shader_path,
                              char *shared_shader_path) {
  char *fragment_shader_shard = load_file(shader_path);
  if (!fragment_shader_shard)
    return false;

  char *shared_fragment_file =
      shared_shader_path ? load_file(shared_shader_path) : NULL;
  char *shared_fragment_shard =
      shared_fragment_file ? shared_fragment_file : "";

  // Create fragment shader
  size_t buf_size =
      strlen(shared_fragment_shard) + strlen(fragment_shader_shard) + 1024;
  char *fragment_shader_source = malloc(buf_size);
  if (!fragment_shader_source) {
    free(fragment_shader_shard);
    free(shared_fragment_file);
    return false;
  }

  snprintf(fragment_shader_source, buf_size, FRAGMENT_SHADER_TEMPLATE,
           shared_fragment_shard, fragment_shader_shard);

  free(fragment_shader_shard);
  free(shared_fragment_file);

  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER_SOURCE);
  GLuint fragment_shader =
      compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

  free(fragment_shader_source);

  if (!vertex_shader || !fragment_shader) {
    if (vertex_shader)
      glDeleteShader(vertex_shader);
    if (fragment_shader)
      glDeleteShader(fragment_shader);
    return false;
  }

  // Link program
  *program = glCreateProgram();
  if (!*program) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return false;
  }

  glAttachShader(*program, vertex_shader);
  glAttachShader(*program, fragment_shader);
  glLinkProgram(*program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint success;
  glGetProgramiv(*program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(*program, 512, NULL, info_log);
    fprintf(stderr, "Program linking failed: %s\n", info_log);
    glDeleteProgram(*program);
    *program = 0;
    return false;
  }

  return true;
}

shader_context *shader_create(struct wl_display *display,
                              struct wl_surface *surface, char *shader_path,
                              char *shared_shader_path, int width, int height,
                              char *channel_input[10]) {
  shader_context *ctx = calloc(1, sizeof(shader_context));
  if (!ctx)
    return NULL;

  // Initialize EGL
  const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  if (!extensions || !strstr(extensions, "EGL_KHR_platform_wayland")) {
    fprintf(stderr, "EGL Wayland platform not supported\n");
    goto error;
  }

  ctx->egl_display =
      eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, NULL);
  if (ctx->egl_display == EGL_NO_DISPLAY)
    goto error;

  if (!eglInitialize(ctx->egl_display, NULL, NULL))
    goto error;
  if (!eglBindAPI(EGL_OPENGL_ES_API))
    goto error;

  // Simple config selection
  EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                             EGL_WINDOW_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES3_BIT,
                             EGL_NONE};

  EGLint num_configs;
  EGLConfig config;
  if (!eglChooseConfig(ctx->egl_display, config_attribs, &config, 1,
                       &num_configs) ||
      num_configs == 0) {
    goto error;
  }
  ctx->egl_config = config;

  // Create context
  EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                              EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};

  ctx->egl_context = eglCreateContext(ctx->egl_display, ctx->egl_config,
                                      EGL_NO_CONTEXT, context_attribs);
  if (ctx->egl_context == EGL_NO_CONTEXT)
    goto error;

  // Create EGL window and surface
  ctx->egl_window = wl_egl_window_create(surface, width, height);
  if (!ctx->egl_window)
    goto error;

  ctx->egl_surface =
      eglCreateWindowSurface(ctx->egl_display, ctx->egl_config,
                             (EGLNativeWindowType)ctx->egl_window, NULL);
  if (ctx->egl_surface == EGL_NO_SURFACE)
    goto error;

  // Make context current
  if (!eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface,
                      ctx->egl_context))
    goto error;

  // Disable vsync for manual timing control
  eglSwapInterval(ctx->egl_display, 0);

  // Create vertex buffer
  glGenVertexArrays(1, &ctx->vao);
  glGenBuffers(1, &ctx->vbo);

  glBindVertexArray(ctx->vao);
  glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Allocate main buffer
  ctx->buf = malloc(sizeof(shader_buffer));
  if (!ctx->buf)
    goto error;
  memset(ctx->buf, 0, sizeof(shader_buffer));

  // Create a registry
  resource_registry *registry = NULL;

  // Add keyboard texture to registry
  shader_channel *channel = malloc(sizeof(shader_channel));
  shader_texture *tex = malloc(sizeof(shader_texture));
  tex->tex_id = ctx->keyboard.tex;
  tex->width = 256;
  tex->height = 3;
  tex->path = NULL;
  channel->tex = tex;
  channel->type = TEXTURE;
  channel->initialized = true;
  registry_add(&registry, "Keyboard", TEXTURE, channel);

  // Parse channel inputs
  for (int i = 0; i < 10; i++) {
    if (!channel_input[i])
      continue;
    ctx->buf->channel[i] = parse_channel_input(channel_input[i], &registry);
  }

  registry_free(registry);

  // Initialize keyboard states
  memset(ctx->keyboard.prev_key, 0, 256);
  memset(ctx->keyboard.key, 0, 256);
  memset(ctx->keyboard.key_toggled, 0, 256);
  // Create keyboard texture
  glGenTextures(1, &ctx->keyboard.tex);
  glBindTexture(GL_TEXTURE_2D, ctx->keyboard.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 3, 0, GL_RED, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Initialize main buffer
  ctx->buf->shader_path = shader_path ? strdup(shader_path) : NULL;
  if (!ctx->buf->shader_path) {
    goto error;
  }
  if (!init_shader_buffer(ctx->buf, width, height, shared_shader_path)) {
    goto error;
  }

  ctx->initialized = true;
  return ctx;

error:
  shader_destroy(ctx);
  return NULL;
}

void shader_render(shader_context *ctx, double current_time, iMouse *mouse) {
  if (!ctx || !ctx->initialized)
    return;

  // Set current key state
  // First 256 - Key down
  // Second 256 - Key just pressed
  // Third 256 - Key toggled
  unsigned char key[768];
  for (int i = 0; i < 256; ++i) {
    key[i] = ctx->keyboard.key[i] * 255;
    key[256 + i] = (ctx->keyboard.prev_key[i] ^ ctx->keyboard.key[i]) * 255;
    if (ctx->keyboard.prev_key[i] == true) {
      // Key was just released
      key[256 + i] = 0;
    } else if (key[256 + i]) {
      // Key was just pressed
      ctx->keyboard.key_toggled[i] = !ctx->keyboard.key_toggled[i];
    }
    key[i + 512] = ctx->keyboard.key_toggled[i] * 255;
    // Update keyboard state for next frame
    ctx->keyboard.prev_key[i] = ctx->keyboard.key[i];
  }

  // Set keyboard texture
  glBindTexture(GL_TEXTURE_2D, ctx->keyboard.tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 3, GL_RED, GL_UNSIGNED_BYTE,
                  key);

  render_shader_buffer(ctx, ctx->buf, current_time, mouse);

  // Make context current
  eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface,
                 ctx->egl_context);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->buf->fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, ctx->buf->width, ctx->buf->height, 0, 0,
                    ctx->buf->width, ctx->buf->height, GL_COLOR_BUFFER_BIT,
                    GL_NEAREST);

  // Swap buffers
  eglSwapBuffers(ctx->egl_display, ctx->egl_surface);
}

void shader_resize(shader_context *ctx, int width, int height) {
  if (!ctx || !ctx->egl_window)
    return;

  ctx->buf->width = width;
  ctx->buf->height = height;
  wl_egl_window_resize(ctx->egl_window, width, height, 0, 0);
}

void shader_destroy(shader_context *ctx) {
  if (!ctx)
    return;

  if (ctx->egl_display) {
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (ctx->vao)
      glDeleteVertexArrays(1, &ctx->vao);
    if (ctx->vbo)
      glDeleteBuffers(1, &ctx->vbo);

    if (ctx->keyboard.tex) {
      glDeleteTextures(1, &ctx->keyboard.tex);
    }

    if (ctx->buf) {
      free_shader_buffer(ctx->buf);
      ctx->buf = NULL;
    }

    if (ctx->egl_surface)
      eglDestroySurface(ctx->egl_display, ctx->egl_surface);
    if (ctx->egl_context)
      eglDestroyContext(ctx->egl_display, ctx->egl_context);
    if (ctx->egl_window)
      wl_egl_window_destroy(ctx->egl_window);

    eglTerminate(ctx->egl_display);
  }

  free(ctx);
}
