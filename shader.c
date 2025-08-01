#define STB_IMAGE_IMPLEMENTATION

#include "shader.h"
#include "stb_image.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-egl.h>

// Simple vertex data - single triangle covering entire screen
static const float vertices[] = {
    -1.0f, -3.0f, // Bottom-left (extends beyond screen)
    -1.0f, 1.0f,  // Top-left
    3.0f,  1.0f   // Top-right (extends beyond screen)
};

static const char *vertex_shader_source =
    "#version 320 es\n"
    "layout(location = 0) in vec2 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static char *load_shader_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  fread(buffer, 1, length, file);
  buffer[length] = '\0';
  fclose(file);
  return buffer;
}

static GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
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

shader_context *shader_create(struct wl_display *display,
                              struct wl_surface *surface,
                              const char *shader_path, int width, int height,
                              char *texture_paths[4]) {
  shader_context *ctx = calloc(1, sizeof(shader_context));
  if (!ctx)
    return NULL;

  ctx->width = width;
  ctx->height = height;

  // Initialize EGL
  const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  if (!strstr(extensions, "EGL_KHR_platform_wayland")) {
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
  if (!eglChooseConfig(ctx->egl_display, config_attribs, &ctx->egl_config, 1,
                       &num_configs) ||
      num_configs == 0) {
    goto error;
  }

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

  // Load and compile shaders
  char *frag_source = load_shader_file(shader_path);
  if (!frag_source)
    goto error;

  // Create fragment shader with Shadertoy compatibility
  char *full_frag_source = malloc(strlen(frag_source) + 512);
  sprintf(full_frag_source,
          "#version 320 es\n"
          "precision highp float;\n"
          "uniform vec3 iResolution;\n"
          "uniform float iTime;\n"
          "uniform vec4 iMouse;\n"
          "uniform vec2 iMousePos;\n"
          "uniform sampler2D iChannel0;\n"
          "uniform sampler2D iChannel1;\n"
          "uniform sampler2D iChannel2;\n"
          "uniform sampler2D iChannel3;\n"
          "out vec4 fragColor;\n"
          "%s\n"
          "void main() {\n"
          "    mainImage(fragColor, gl_FragCoord.xy);\n"
          "}\n",
          frag_source);

  free(frag_source);

  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, full_frag_source);

  free(full_frag_source);

  if (!vertex_shader || !fragment_shader) {
    if (vertex_shader)
      glDeleteShader(vertex_shader);
    if (fragment_shader)
      glDeleteShader(fragment_shader);
    goto error;
  }

  // Link program
  ctx->program = glCreateProgram();
  glAttachShader(ctx->program, vertex_shader);
  glAttachShader(ctx->program, fragment_shader);
  glLinkProgram(ctx->program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  GLint success;
  glGetProgramiv(ctx->program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(ctx->program, 512, NULL, info_log);
    fprintf(stderr, "Program linking failed: %s\n", info_log);
    goto error;
  }

  // Create vertex buffer
  glGenVertexArrays(1, &ctx->vao);
  glGenBuffers(1, &ctx->vbo);

  glBindVertexArray(ctx->vao);
  glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Get uniform locations
  ctx->u_resolution = glGetUniformLocation(ctx->program, "iResolution");
  ctx->u_time = glGetUniformLocation(ctx->program, "iTime");
  ctx->u_mouse = glGetUniformLocation(ctx->program, "iMouse");
  ctx->u_mouse_pos = glGetUniformLocation(ctx->program, "iMousePos");

  // Get uniform locations for iChannel0-3
  char name[16];
  for (int i = 0; i < 4; i++) {
    snprintf(name, sizeof(name), "iChannel%d", i);
    ctx->u_textures[i] = glGetUniformLocation(ctx->program, name);
  }

  // Bind textures for all channels
  for (int i = 0; i < 4; i++) {
    // Load texture if path is specified
    char *path = texture_paths[i];
    if (!path)
      continue;

    int width;
    int height;
    unsigned char *data =
        stbi_load(path, &width, &height, NULL, STBI_rgb_alpha);
    if (!data) {
      fprintf(stderr,
              "Texture could not be loaded at %s, is there a file there?\n",
              path);
      continue;
    }

    shader_texture *texture = malloc(sizeof(shader_texture));
    texture->data = data;
    texture->width = width;
    texture->height = height;
    ctx->textures[i] = texture;

    glGenTextures(1, &ctx->texture_names[i]);
    glBindTexture(GL_TEXTURE_2D, ctx->texture_names[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx->textures[i]->width,
                 ctx->textures[i]->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 ctx->textures[i]->data);
  }

  ctx->initialized = true;
  return ctx;

error:
  shader_destroy(ctx);
  return NULL;
}

void shader_render(shader_context *ctx, double time, iMouse *mouse,
                   unsigned char *image_data, int image_width,
                   int image_height) {
  if (!ctx || !ctx->initialized)
    return;

  // Make context current
  eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface,
                 ctx->egl_context);

  // Set viewport
  glViewport(0, 0, ctx->width, ctx->height);

  // Use shader program
  glUseProgram(ctx->program);

  // Bind textures
  for (int i = 0; i < 4; ++i) {
    if (!ctx->textures[i] || !ctx->texture_names[i])
      continue;

    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, ctx->texture_names[i]);

    // Set sampler uniform
    if (ctx->u_textures[i] >= 0) {
      glUniform1i(ctx->u_textures[i], i);
    }
  }

  if (ctx->u_resolution >= 0) {
    glUniform3f(ctx->u_resolution, (float)ctx->width, (float)ctx->height,
                (float)ctx->width / ctx->height);
  }
  if (ctx->u_time >= 0) {
    glUniform1f(ctx->u_time, (float)time);
  }
  if (mouse) {
    if (ctx->u_mouse >= 0) {
      glUniform4f(ctx->u_mouse, mouse->x, mouse->y, mouse->z, mouse->w);
    }
    if (ctx->u_mouse_pos >= 0) {
      glUniform2f(ctx->u_mouse_pos, mouse->real_x, mouse->real_y);
    }
  }

  // Draw
  glBindVertexArray(ctx->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Swap buffers
  eglSwapBuffers(ctx->egl_display, ctx->egl_surface);
}

void shader_resize(shader_context *ctx, int width, int height) {
  if (!ctx || !ctx->egl_window)
    return;

  ctx->width = width;
  ctx->height = height;
  wl_egl_window_resize(ctx->egl_window, width, height, 0, 0);
}

void shader_destroy(shader_context *ctx) {
  if (!ctx)
    return;

  if (ctx->egl_display) {
    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (ctx->program)
      glDeleteProgram(ctx->program);

    for (int i = 0; i < 4; ++i) {
      if (!ctx->textures[i])
        continue;
      stbi_image_free(ctx->textures[i]->data);
      free(ctx->textures[i]);
      ctx->textures[i] = NULL;
    }
    glDeleteTextures(4, ctx->texture_names);

    if (ctx->vao)
      glDeleteVertexArrays(1, &ctx->vao);
    if (ctx->vbo)
      glDeleteBuffers(1, &ctx->vbo);

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
