#include "shader_channel.h"

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
static shader_channel *parse_token(const char *input, int *pos) {
  skip_whitespace(input, pos);
  if (!input[*pos])
    return NULL;

  if (input[*pos] == '(') {
    // Parse nested expression
    (*pos)++;
    shader_channel *token[5] = {0};
    int count = 0;

    while (input[*pos] && input[*pos] != ')') {
      if (count >= 5)
        break;
      shader_channel *current_token = parse_token(input, pos);
      if (!current_token)
        break;
      token[count++] = current_token;
    }

    if (input[*pos] != ')') {
      fprintf(stderr, "Invalid shader buffer syntax or too many channels for "
                      "shader buffer (>4).");
      exit(EXIT_FAILURE);
    }
    (*pos)++;

    // Only keep the last token as main channel
    shader_channel *last_token = token[count - 1];
    if (last_token->type != BUFFER) {
      fprintf(stderr, "Last argument must be a buffer to assign channels.");
      exit(EXIT_FAILURE);
    }

    // Copy channels to buffer
    for (int i = 0; i < count - 1; i++) {
      last_token->buf->channel[i] = token[i];
    }

    return last_token;
  }

  // Parse type prefix
  char type_char = input[*pos];
  if (type_char != 't' && type_char != 'b')
    return NULL;
  (*pos)++;

  if (input[*pos] != ':')
    return NULL;
  (*pos)++;

  shader_channel *channel = malloc(sizeof(shader_channel));

  // Parse path
  int start = *pos;
  while (input[*pos] && !isspace(input[*pos]) && input[*pos] != ')') {
    (*pos)++;
  }
  int len = *pos - start;
  char *path = malloc(len + 1);
  strncpy(path, input + start, len);
  path[len] = '\0';

  if (type_char == 't') {
    channel->tex = malloc(sizeof(shader_texture));
    channel->type = TEXTURE;
    channel->tex->path = path;
  } else {
    channel->buf = malloc(sizeof(shader_buffer));
    channel->type = BUFFER;
    channel->buf->shader_path = path;
  }

  return channel;
}

// Main parser function
shader_channel *parse_channel_input(const char *input) {
  int pos = 0, count = 0;
  shader_channel *channel[5];
  shader_channel *current_token, *last_token;

  while ((current_token = parse_token(input, &pos))) {
    if (count > 4) {
      fprintf(stderr, "Too many channels for shader buffer (>4).");
      exit(EXIT_FAILURE);
    }
    channel[count++] = current_token;
  }

  // Only keep the last token as main channel
  last_token = channel[count - 1];
  if (last_token->type != BUFFER) {
    fprintf(stderr, "Last argument must be a buffer to assign channels.");
    exit(EXIT_FAILURE);
  }

  // Copy channels to buffer
  for (int i = 0; i < count - 1; i++) {
    last_token->buf->channel[i] = channel[i];
  }

  return last_token;
}

// Recursive function to free channel structure
void free_shader_channel(shader_channel *channel) {
  if (!channel)
    return;

  if (channel->type == TEXTURE) {
    free(channel->tex->path);
    free(channel->tex);
  } else if (channel->type == BUFFER) {
    for (int i = 0; i < 4; i++) {
      if (channel->buf->channel[i]) {
        free_shader_channel(channel->buf->channel[i]);
      }
    }
    free(channel->buf->shader_path);
    free(channel->buf);
  }

  free(channel);
}
