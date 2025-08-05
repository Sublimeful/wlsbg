#ifndef H_RESOURCE_REGISTRY
#define H_RESOURCE_REGISTRY

#include "shader_channel.h"

struct _shader_channel;
typedef struct _shader_channel shader_channel;

typedef struct _resource_registry resource_registry;

struct _resource_registry {
  char *name;
  shader_channel_type type;
  shader_channel *channel;
  resource_registry *next;
};

void registry_add(resource_registry **registry, const char *name,
                  shader_channel_type type, shader_channel *channel);
shader_channel *registry_lookup(resource_registry *registry, const char *name,
                                shader_channel_type type);
void registry_free(resource_registry *registry);

#endif
