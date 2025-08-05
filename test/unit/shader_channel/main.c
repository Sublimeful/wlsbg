#include "shader_buffer.h"
#include "shader_channel.h"
#include "shader_texture.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void parse_channel_input_1() {
  char *input = "t:image1.png (t:image2.png t:image3.png b:shader1.frag) "
                "b:shader2.frag";
  // Create a registry
  resource_registry *registry = NULL;
  shader_channel *channel = parse_channel_input(input, &registry);
  free(registry);

  assert(channel->type == BUFFER);
  assert(strcmp(channel->buf->shader_path, "shader2.frag") == 0);

  assert(channel->buf->channel[0]->type == TEXTURE);
  assert(strcmp(channel->buf->channel[0]->tex->path, "image1.png") == 0);

  assert(channel->buf->channel[1]->type == BUFFER);
  assert(strcmp(channel->buf->channel[1]->buf->shader_path, "shader1.frag") ==
         0);

  assert(channel->buf->channel[1]->buf->channel[0]->type == TEXTURE);
  assert(strcmp(channel->buf->channel[1]->buf->channel[0]->tex->path,
                "image2.png") == 0);

  assert(channel->buf->channel[1]->buf->channel[1]->type == TEXTURE);
  assert(strcmp(channel->buf->channel[1]->buf->channel[1]->tex->path,
                "image3.png") == 0);
};

void parse_channel_input_2() {
  char *input = "t:image1.png b:shader1.frag b:shader2.frag";
  // Create a registry
  resource_registry *registry = NULL;
  shader_channel *channel = parse_channel_input(input, &registry);
  free(registry);

  assert(channel->type == BUFFER);
  assert(strcmp(channel->buf->shader_path, "shader2.frag") == 0);

  assert(channel->buf->channel[0]->type == TEXTURE);
  assert(strcmp(channel->buf->channel[0]->tex->path, "image1.png") == 0);

  assert(channel->buf->channel[1]->type == BUFFER);
  assert(strcmp(channel->buf->channel[1]->buf->shader_path, "shader1.frag") ==
         0);
};

int main() {
  parse_channel_input_1();
  parse_channel_input_2();
}
