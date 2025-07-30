#include "shader.h"
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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

  // Load and compile shaders
  char *shader_contents = load_file(shader_path);
  if (!shader_contents) {
    goto error;
  }

  // Create fragment_src
  char *fragment_preamble = "#version 320 es\n"
                            "precision highp float;\n"
                            "uniform sampler2D iTexture;\n"
                            "layout(std140, binding = 0) uniform ShaderData {\n"
                            "    vec3 iResolution;\n"
                            "    float iTime;\n"
                            "    vec4 iMouse;\n"
                            "    vec2 iMousePos;\n"
                            "};\n";
  unsigned int fragment_length =
      strlen(fragment_preamble) + strlen(shader_contents) + 1;
  char *fragment_src = (char *)malloc(sizeof(char) * fragment_length);
  strcpy(fragment_src, fragment_preamble);
  strcat(fragment_src, shader_contents);
  free(shader_contents);

  const char *vertex_src = "#version 320 es\n"
                           "layout(location=0) in vec4 position;\n"
                           "layout(location=1) in vec2 texCoord;\n"
                           "out vec2 vTexCoord;\n"
                           "void main() {\n"
                           "    gl_Position = position;\n"
                           "    vTexCoord = texCoord;\n"
                           "}\n";

  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);

  // Free fragment_src
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
      // x,    y,     u,    v
      -1.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
      1.0f,  -1.0f, 1.0f, 0.0f, // Bottom-right
      -1.0f, 1.0f,  0.0f, 1.0f, // Top-left
      1.0f,  1.0f,  1.0f, 1.0f  // Top-right
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

  // Create UBO
  glGenBuffers(1, &ctx->ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ctx->ubo);
  glBufferData(GL_UNIFORM_BUFFER, 64, NULL, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  // Bind uniform block
  GLuint block_index = glGetUniformBlockIndex(ctx->program, "ShaderData");
  if (block_index != GL_INVALID_INDEX) {
    glUniformBlockBinding(ctx->program, block_index, 0);
  }

  return ctx;

error:
  shader_context_destroy(ctx);
  return NULL;
}

void shader_render(shader_context *ctx, cairo_surface_t *input,
                   cairo_surface_t *output, double time, float mouse_x,
                   float mouse_y, float down_x, float down_y, float click_x,
                   float click_y, bool is_down, bool is_clicked) {
  if (!ctx || !input || !output)
    return;

  // Make context current
  eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

  // Clear any existing errors
  while (glGetError() != GL_NO_ERROR)
    ;

  // Update texture with input surface
  unsigned char *data = cairo_image_surface_get_data(input);
  int out_width = cairo_image_surface_get_width(output);
  int out_height = cairo_image_surface_get_height(output);

  glBindTexture(GL_TEXTURE_2D, ctx->texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->width, ctx->height, GL_BGRA,
                  GL_UNSIGNED_BYTE, data);

  // Check for texture update errors
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf(stderr, "Texture update error: 0x%x\n", err);
  }

  // Use program
  glUseProgram(ctx->program);

  // Set uniforms
  GLint texLoc = glGetUniformLocation(ctx->program, "iTexture");
  if (texLoc != -1) {
    glUniform1i(texLoc, 0);
  }

  // Create UBO data structure
  typedef struct {
    float resolution[3];
    float time;
    float mouse[4];
    float mouse_pos[2];
    float padding[2]; // Ensure 16-byte alignment
  } ShaderData;

  ShaderData shader_data = {.resolution = {(float)out_width, (float)out_height,
                                           (float)out_width / out_height},
                            .time = (float)time,
                            .mouse = {down_x, down_y,
                                      is_down ? click_x : -click_x,
                                      is_clicked ? click_y : -click_y},
                            .mouse_pos = {mouse_x, mouse_y},
                            .padding = {0.0f, 0.0f}};

  // Update UBO
  glBindBuffer(GL_UNIFORM_BUFFER, ctx->ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ShaderData), &shader_data);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ctx->ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  glBindVertexArray(ctx->vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Read back to output surface
  unsigned char *out_data = cairo_image_surface_get_data(output);
  glReadPixels(0, 0, out_width, out_height, GL_BGRA, GL_UNSIGNED_BYTE,
               out_data);

  // Synchronize GPU operations
  glFinish();

  // Check for read errors
  err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf(stderr, "ReadPixels error: 0x%x\n", err);
  }

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

  if (ctx->vao) {
    glDeleteVertexArrays(1, &ctx->vao);
  }

  if (ctx->vbo) {
    glDeleteBuffers(1, &ctx->vbo);
  }

  free(ctx);
}
