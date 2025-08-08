#include "resource_registry.h"
#include "shader_channel.h"
#include <stdlib.h>
#include <string.h>

void registry_add(resource_registry **registry, const char *name,
                  shader_channel_type type, shader_channel *channel) {
  resource_registry *entry = malloc(sizeof(resource_registry));
  entry->name = name ? strdup(name) : NULL;
  entry->type = type;
  entry->channel = channel;
  entry->next = *registry;
  *registry = entry;
}

shader_channel *registry_lookup(resource_registry *registry, const char *name,
                                shader_channel_type type) {
  for (resource_registry *cur = registry; cur; cur = cur->next) {
    if (strcmp(cur->name, name) == 0 && cur->type == type) {
      return cur->channel;
    }
  }
  return NULL;
}

bool registry_contains_channel(resource_registry *registry,
                               shader_channel *channel) {
  for (resource_registry *cur = registry; cur; cur = cur->next) {
    if (cur->channel == channel)
      return true;
  }
  return false;
}

void registry_free(resource_registry *registry, bool free_shader_channels) {
  while (registry) {
    resource_registry *next = registry->next;
    if (free_shader_channels && registry->channel) {
      free_shader_channel(registry->channel);
      registry->channel = NULL;
    }
    free(registry->name);
    free(registry);
    registry = next;
  }
}
