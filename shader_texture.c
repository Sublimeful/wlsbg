#define STB_IMAGE_IMPLEMENTATION

#include "shader_texture.h"
#include "stb_image.h"

bool load_shader_texture(shader_texture *tex) {
  int width, height;
  stbi_set_flip_vertically_on_load(true);
  unsigned char *data =
      stbi_load(tex->path, &width, &height, NULL, STBI_rgb_alpha);
  if (!data) {
    fprintf(stderr, "Error: Could not load texture from source path '%s'\n",
            tex->path);
    return false;
  }

  tex->width = width;
  tex->height = height;

  glGenTextures(1, &tex->tex_id);
  glBindTexture(GL_TEXTURE_2D, tex->tex_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(data);

  return true;
}
