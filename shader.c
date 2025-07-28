#include "shader.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

// Function to check EGL errors
static void check_egl_error(const char *msg) {
  EGLint error = eglGetError();
  if (error != EGL_SUCCESS) {
    fprintf(stderr, "%s: EGL error 0x%04X\n", msg, error);
  }
}

void apply_shader(cairo_surface_t *surface, const char *shader_path) {
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);

  // Initialize EGL
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    fprintf(stderr, "Failed to get EGL display\n");
    return;
  }

  EGLint major, minor;
  if (!eglInitialize(display, &major, &minor)) {
    fprintf(stderr, "Failed to initialize EGL\n");
    return;
  }

  printf("EGL initialized: %d.%d\n", major, minor);

  // Choose EGL config
  const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_ES3_BIT,
                                   EGL_SURFACE_TYPE,
                                   EGL_PBUFFER_BIT,
                                   EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_ALPHA_SIZE,
                                   8,
                                   EGL_DEPTH_SIZE,
                                   0,
                                   EGL_STENCIL_SIZE,
                                   0,
                                   EGL_NONE};

  EGLConfig config;
  EGLint num_configs;
  if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) ||
      num_configs == 0) {
    fprintf(stderr, "Failed to choose EGL config\n");
    eglTerminate(display);
    return;
  }

  // Create a dummy pbuffer surface
  const EGLint pbuffer_attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height,
                                    EGL_NONE};

  EGLSurface egl_surface =
      eglCreatePbufferSurface(display, config, pbuffer_attribs);
  if (egl_surface == EGL_NO_SURFACE) {
    fprintf(stderr, "Failed to create EGL surface\n");
    check_egl_error("eglCreatePbufferSurface");
    eglTerminate(display);
    return;
  }

  // Bind GLES API
  eglBindAPI(EGL_OPENGL_ES_API);

  // Create context
  const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                                    EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};

  EGLContext context =
      eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    fprintf(stderr, "Failed to create EGL context\n");
    check_egl_error("eglCreateContext");
    eglDestroySurface(display, egl_surface);
    eglTerminate(display);
    return;
  }

  // Make context current
  if (!eglMakeCurrent(display, egl_surface, egl_surface, context)) {
    fprintf(stderr, "Failed to make EGL context current\n");
    check_egl_error("eglMakeCurrent");
    eglDestroyContext(display, context);
    eglDestroySurface(display, egl_surface);
    eglTerminate(display);
    return;
  }

  printf("OpenGL ES %s\n", glGetString(GL_VERSION));

  // Create texture from Cairo surface
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Allocate texture storage
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);

  // Create temporary buffer for RGBA data
  uint8_t *rgba_data = malloc(width * height * 4);
  if (!rgba_data) {
    fprintf(stderr, "Memory allocation failed\n");
    // Cleanup and return
    glDeleteTextures(1, &texture);
    eglDestroyContext(display, context);
    eglDestroySurface(display, egl_surface);
    eglTerminate(display);
    return;
  }

  // Convert BGRA to RGBA (Cairo's format to OpenGL's format)
  for (int i = 0; i < width * height; i++) {
    rgba_data[i * 4 + 0] = data[i * 4 + 2]; // R
    rgba_data[i * 4 + 1] = data[i * 4 + 1]; // G
    rgba_data[i * 4 + 2] = data[i * 4 + 0]; // B
    rgba_data[i * 4 + 3] = data[i * 4 + 3]; // A
  }

  // Upload texture data
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                  GL_UNSIGNED_BYTE, rgba_data);
  free(rgba_data);

  // Create framebuffer
  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);

  // Check framebuffer status
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Framebuffer is not complete\n");
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    eglDestroyContext(display, context);
    eglDestroySurface(display, egl_surface);
    eglTerminate(display);
    return;
  }

  // Load shader source
  char *shader_source = load_file(shader_path);
  if (!shader_source) {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    eglDestroyContext(display, context);
    eglDestroySurface(display, egl_surface);
    eglTerminate(display);
    return;
  }

  // Compile shaders
  const char *vertex_shader_src = "#version 300 es\n"
                                  "layout(location=0) in vec4 position;\n"
                                  "layout(location=1) in vec2 texCoord;\n"
                                  "out vec2 vTexCoord;\n"
                                  "void main() {\n"
                                  "    gl_Position = position;\n"
                                  "    vTexCoord = texCoord;\n"
                                  "}\n";

  // Vertex shader
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_src, NULL);
  glCompileShader(vertex_shader);

  GLint success;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
    fprintf(stderr, "Vertex shader compilation failed:\n%s\n", info_log);
  }

  // Fragment shader
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, (const char *const *)&shader_source, NULL);
  glCompileShader(fragment_shader);

  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
    fprintf(stderr, "Fragment shader compilation failed:\n%s\n", info_log);
  }

  free(shader_source);

  // Create shader program
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(program, 512, NULL, info_log);
    fprintf(stderr, "Shader linking failed:\n%s\n", info_log);
  }

  // Set up vertex data
  GLfloat vertices[] = {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                        -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};

  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        (void *)0);
  glEnableVertexAttribArray(0);

  // Texture coordinate attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                        (void *)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  // Render
  glViewport(0, 0, width, height);

  glUseProgram(program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  // Get and set the texture uniform location
  GLint texLoc = glGetUniformLocation(program, "u_texture");
  if (texLoc == -1) {
    fprintf(stderr, "u_texture uniform not found!\n");
  } else {
    glUniform1i(texLoc, 0); // Texture unit 0
  }

  // Set resolution uniform
  GLint resLoc = glGetUniformLocation(program, "iResolution");
  if (resLoc != -1) {
    glUniform2f(resLoc, (float)width, (float)height);
  }

  // Set time uniform
  static float time = 0.0f;
  time += 0.016f; // ~60fps delta
  printf("Time: %f\n", time);
  GLint timeLoc = glGetUniformLocation(program, "iTime");
  if (timeLoc != -1) {
    glUniform1f(timeLoc, time);
  }

  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Read back to surface
  uint8_t *readback_data = malloc(width * height * 4);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readback_data);

  // Convert back to Cairo's BGRA format
  for (int i = 0; i < width * height; i++) {
    data[i * 4 + 0] = readback_data[i * 4 + 2]; // B
    data[i * 4 + 1] = readback_data[i * 4 + 1]; // G
    data[i * 4 + 2] = readback_data[i * 4 + 0]; // R
    data[i * 4 + 3] = readback_data[i * 4 + 3]; // A
  }
  free(readback_data);

  // Clean up
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteTextures(1, &texture);
  glDeleteFramebuffers(1, &fbo);

  eglDestroyContext(display, context);
  eglDestroySurface(display, egl_surface);
  eglTerminate(display);

  cairo_surface_mark_dirty(surface);
}
