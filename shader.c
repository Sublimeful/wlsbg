#include "shader.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct shader_context {
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
  GLuint program;
  GLuint texture;
  GLuint fbo;
  GLuint vao;
  GLuint vbo;
  int width;
  int height;
} shader_context;

// Function to load a file into memory
static char *load_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    perror("Failed to open shader file");
    return NULL;
  }

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
    fprintf(stderr, "Shader compilation failed:\n%s\n", info_log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

shader_context *shader_context_create(const char *shader_path, int width,
                                      int height) {
  shader_context *ctx = calloc(1, sizeof(shader_context));
  if (!ctx)
    return NULL;

  ctx->width = width;
  ctx->height = height;

  // Initialize EGL
  ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (ctx->display == EGL_NO_DISPLAY) {
    fprintf(stderr, "Failed to get EGL display\n");
    goto error;
  }

  EGLint major, minor;
  if (!eglInitialize(ctx->display, &major, &minor)) {
    fprintf(stderr, "Failed to initialize EGL\n");
    goto error;
  }

  // Choose config
  EGLConfig config;
  EGLint num_configs;
  if (!eglChooseConfig(ctx->display, NULL, &config, 1, &num_configs) ||
      num_configs == 0) {
    fprintf(stderr, "Failed to choose EGL config\n");
    goto error;
  }

  // Create pbuffer surface
  const EGLint pbuffer_attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height,
                                    EGL_NONE};

  ctx->surface = eglCreatePbufferSurface(ctx->display, config, pbuffer_attribs);
  if (ctx->surface == EGL_NO_SURFACE) {
    fprintf(stderr, "Failed to create EGL surface\n");
    goto error;
  }

  // Bind API and create context
  eglBindAPI(EGL_OPENGL_ES_API);

  const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                    EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};

  ctx->context =
      eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, context_attribs);
  if (ctx->context == EGL_NO_CONTEXT) {
    fprintf(stderr, "Failed to create EGL context\n");
    goto error;
  }

  if (!eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context)) {
    fprintf(stderr, "Failed to make context current\n");
    goto error;
  }

  // Create texture
  glGenTextures(1, &ctx->texture);
  glBindTexture(GL_TEXTURE_2D, ctx->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);

  // Create FBO
  glGenFramebuffers(1, &ctx->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         ctx->texture, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Framebuffer incomplete\n");
    goto error;
  }

  // Load and compile shaders
  char *fragment_src = load_file(shader_path);
  if (!fragment_src) {
    goto error;
  }

  const char *vertex_src = "#version 300 es\n"
                           "layout(location=0) in vec4 position;\n"
                           "layout(location=1) in vec2 texCoord;\n"
                           "out vec2 vTexCoord;\n"
                           "void main() {\n"
                           "    gl_Position = position;\n"
                           "    vTexCoord = texCoord;\n"
                           "}\n";

  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
  free(fragment_src);

  if (!vertex_shader || !fragment_shader) {
    if (vertex_shader)
      glDeleteShader(vertex_shader);
    if (fragment_shader)
      glDeleteShader(fragment_shader);
    goto error;
  }

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
    fprintf(stderr, "Program linking failed:\n%s\n", info_log);
    goto error;
  }

  // Set up vertex data
  GLfloat vertices[] = {
      // x,    y,     u,    v (flipped vertically)
      -1.0f, -1.0f, 0.0f, 1.0f, // Bottom-left
      1.0f,  -1.0f, 1.0f, 1.0f, // Bottom-right
      -1.0f, 1.0f,  0.0f, 0.0f, // Top-left
      1.0f,  1.0f,  1.0f, 0.0f  // Top-right
  };

  glGenVertexArrays(1, &ctx->vao);
  glGenBuffers(1, &ctx->vbo);

  glBindVertexArray(ctx->vao);
  glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        (void *)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  return ctx;

error:
  shader_context_destroy(ctx);
  return NULL;
}

void shader_render(shader_context *ctx, cairo_surface_t *input,
                   cairo_surface_t *output, float time, float mouse_x,
                   float mouse_y, float down_x, float down_y, float click_x,
                   float click_y, bool is_down, bool is_clicked) {
  if (!ctx || !input || !output)
    return;

  // Make context current
  eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

  // Update texture with input surface
  unsigned char *data = cairo_image_surface_get_data(input);
  int out_width = cairo_image_surface_get_width(output);
  int out_height = cairo_image_surface_get_height(output);

  glBindTexture(GL_TEXTURE_2D, ctx->texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->width, ctx->height, GL_RGBA,
                  GL_UNSIGNED_BYTE, data);

  // Render
  glUseProgram(ctx->program);

  // Set uniforms
  GLint texLoc = glGetUniformLocation(ctx->program, "iTexture");
  if (texLoc != -1) {
    glUniform1i(texLoc, 0);
  }

  GLint mouseLoc = glGetUniformLocation(ctx->program, "iMouse");
  if (mouseLoc != -1) {
    float z = is_down ? click_x : -click_x;
    float w = is_clicked ? click_y : -click_y;
    glUniform4f(mouseLoc, down_x, down_y, z, w);
  }

  GLint mousePosLoc = glGetUniformLocation(ctx->program, "iMousePos");
  if (mousePosLoc != -1) {
    glUniform2f(mousePosLoc, mouse_x, mouse_y);
  }

  GLint mouseDownLoc = glGetUniformLocation(ctx->program, "iMouseDownPos");
  if (mouseDownLoc != -1) {
    float x = is_down ? down_x : -down_x;
    float y = is_down ? down_y : -down_y;
    glUniform2f(mouseDownLoc, x, y);
  }

  GLint mouseClickLoc = glGetUniformLocation(ctx->program, "iMouseClickPos");
  if (mouseClickLoc != -1) {
    int x = is_clicked ? click_x : -click_x;
    int y = is_clicked ? click_y : -click_y;
    glUniform2f(mouseClickLoc, x, y);
  }

  GLint resLoc = glGetUniformLocation(ctx->program, "iResolution");
  if (resLoc != -1) {
    float aspect_ratio = (float)out_width / out_height;
    glUniform3f(resLoc, out_width, out_height, aspect_ratio);
  }

  GLint timeLoc = glGetUniformLocation(ctx->program, "iTime");
  if (timeLoc != -1) {
    glUniform1f(timeLoc, time);
  }

  glBindVertexArray(ctx->vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Read back to output surface
  unsigned char *out_data = cairo_image_surface_get_data(output);
  glReadPixels(0, 0, out_width, out_height, GL_RGBA, GL_UNSIGNED_BYTE,
               out_data);

  cairo_surface_mark_dirty(output);
}

void shader_context_destroy(shader_context *ctx) {
  if (!ctx)
    return;

  if (ctx->display) {
    eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (ctx->context) {
      eglDestroyContext(ctx->display, ctx->context);
    }

    if (ctx->surface) {
      eglDestroySurface(ctx->display, ctx->surface);
    }

    eglTerminate(ctx->display);
  }

  if (ctx->program) {
    glDeleteProgram(ctx->program);
  }

  if (ctx->texture) {
    glDeleteTextures(1, &ctx->texture);
  }

  if (ctx->fbo) {
    glDeleteFramebuffers(1, &ctx->fbo);
  }

  if (ctx->vao) {
    glDeleteVertexArrays(1, &ctx->vao);
  }

  if (ctx->vbo) {
    glDeleteBuffers(1, &ctx->vbo);
  }

  free(ctx);
}
