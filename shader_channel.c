#include "shader_channel.h"
#include "resource_registry.h"
#include "shader.h"
#include "shader_buffer.h"
#include "shader_texture.h"
#include "shader_uniform.h"
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to skip whitespace
static void skip_whitespace(const char *input, int *pos) {
  while (input[*pos] && isspace(input[*pos])) {
    (*pos)++;
  }
}

// Function to parse a token
shader_channel *parse_token(const char *input, int *pos,
                            resource_registry **registry) {
  skip_whitespace(input, pos);
  if (!input[*pos])
    return NULL;

  if (input[*pos] == '(') {
    // Parse nested expression
    (*pos)++;
    shader_channel *token[11] = {0};
    int count = 0;

    while (input[*pos] && input[*pos] != ')') {
      if (count >= 11)
        break;
      shader_channel *current_token = parse_token(input, pos, registry);
      if (!current_token)
        break;
      token[count++] = current_token;
    }

    if (input[*pos] != ')') {
      fprintf(stderr, "Invalid shader buffer syntax or too many channels for "
                      "shader buffer (>10).\n");
      exit(EXIT_FAILURE);
    }
    (*pos)++;

    // Only keep the last token as main channel
    shader_channel *last_token = token[count - 1];
    if (count > 1 && last_token->type != BUFFER) {
      fprintf(stderr, "Last argument must be a buffer to assign channels.\n");
      exit(EXIT_FAILURE);
    }

    // Copy channels to buffer
    for (int i = 0; i < count - 1; i++) {
      last_token->buf->channel[i] = token[i];
    }

    return last_token;
  }

  // Parse type prefix (t or b)
  char type_char = input[*pos];
  if (type_char != 't' && type_char != 'b')
    return NULL;
  (*pos)++;

  // Set type
  shader_channel_type type = NONE;
  switch (type_char) {
  case 't':
    type = TEXTURE;
    break;
  case 'b':
    type = BUFFER;
    break;
  default:
    break;
  }

  // Parse name (or empty for anonymous)
  int name_start = *pos;
  while (input[*pos] && !isspace(input[*pos]) && input[*pos] != ':' &&
         input[*pos] != ')') {
    (*pos)++;
  }
  int name_len = *pos - name_start;
  char *name =
      name_len > 0 ? strndup(input + name_start, name_len) : strdup("");

  // Handle definition (with colon) or reference (without colon)
  shader_channel *channel = NULL;
  if (input[*pos] == ':') {
    (*pos)++; // Skip colon
    // Parse path
    int path_start = *pos;
    while (input[*pos] && !isspace(input[*pos]) && input[*pos] != ')') {
      (*pos)++;
    }
    int path_len = *pos - path_start;
    char *path = strndup(input + path_start, path_len);

    // Create resource
    channel = malloc(sizeof(shader_channel));
    channel->initialized = false;
    channel->type = type;

    switch (type) {
    case TEXTURE:
      channel->tex = malloc(sizeof(shader_texture));
      channel->tex->path = path;
      break;
    case BUFFER:
      channel->buf = calloc(1, sizeof(shader_buffer));
      channel->buf->shader_path = path;
      break;
    default:
      break;
    }

    if (name_len > 0) {
      // Can not define multiple resources with the same name and type
      if (registry_lookup(*registry, name, type)) {
        fprintf(stderr, "Error: Resource '%s' already defined\n", name);
        exit(EXIT_FAILURE);
      }
      registry_add(registry, name, type, channel);
    } else {
      registry_add(registry, NULL, type, channel);
    }
  } else {
    // Reference existing resource
    if (name_len == 0) {
      fprintf(stderr, "Error: Reference must include a name\n");
      exit(EXIT_FAILURE);
    }
    resource_registry *existing_registry =
        registry_lookup(*registry, name, type);
    if (!existing_registry) {
      fprintf(stderr, "Error: Resource '%s' not found\n", name);
      exit(EXIT_FAILURE);
    }
    channel = existing_registry->channel;
    existing_registry->referenced = true;
  }
  free(name);
  return channel;
}

// Main parser function
shader_channel *parse_channel_input(const char *input,
                                    resource_registry **registry_pointer) {
  int pos = 0, count = 0;
  shader_channel *channel[11] = {0};

  while (count < 11) {
    shader_channel *token = parse_token(input, &pos, registry_pointer);
    if (!token)
      break;
    channel[count++] = token;
  }

  shader_channel *last_token = channel[count - 1];
  for (int i = 0; i < count - 1; i++) {
    if (last_token->type == BUFFER) {
      last_token->buf->channel[i] = channel[i];
    }
  }

  return last_token;
}

void free_shader_channel(shader_channel *channel) {
  if (!channel)
    return;

  switch (channel->type) {
  case TEXTURE:
    glDeleteTextures(1, &channel->tex->tex_id);
    free(channel->tex->path);
    free(channel->tex);
    break;
  case BUFFER:
    free_shader_buffer(channel->buf);
    break;
  default:
    break;
  }

  free(channel);
}

// Get texture ID from any channel type
GLuint get_channel_texture(shader_channel *channel) {
  if (channel->type == TEXTURE) {
    return channel->tex->tex_id;
  } else if (channel->type == BUFFER) {
    return channel->buf->textures[channel->buf->current_texture];
  }
  return 0;
}

bool init_channel_recursive(shader_channel *channel, int width, int height,
                            char *shared_shader_path) {
  if (!channel)
    return false;

  if (channel->initialized)
    return true;
  channel->initialized = true;

  switch (channel->type) {
  case TEXTURE:
    if (!load_shader_texture(channel->tex)) {
      return false;
    }
    break;
  case BUFFER:
    if (!init_shader_buffer(channel->buf, width, height, shared_shader_path)) {
      return false;
    }
    break;
  default:
    break;
  }
  return true;
}
