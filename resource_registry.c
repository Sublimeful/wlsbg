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
  entry->referenced = false;
  *registry = entry;
}

resource_registry *registry_lookup(resource_registry *registry,
                                   const char *name, shader_channel_type type) {
  for (resource_registry *cur = registry; cur; cur = cur->next) {
    if (!cur->name)
      continue;
    if (strcmp(cur->name, name) == 0 && cur->type == type) {
      return cur;
    }
  }
  return NULL;
}

void registry_free(resource_registry *registry) {
  while (registry) {
    resource_registry *next = registry->next;
    free_shader_channel(registry->channel);
    free(registry->name);
    free(registry);
    registry = next;
  }
}
