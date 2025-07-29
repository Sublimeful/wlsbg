#include "background-image.h"
#include "cairo.h"
#include "cairo_util.h"
#include "fractional-scale-v1-client-protocol.h"
#include "log.h"
#include "pool-buffer.h"
#include "shader.h"
#include "single-pixel-buffer-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <assert.h>
#include <bits/getopt_core.h>
#include <ctype.h>
#include <getopt.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

/*
 * If `color` is a hexadecimal string of the form 'rrggbb' or '#rrggbb',
 * `*result` will be set to the uint32_t version of the color. Otherwise,
 * return false and leave `*result` unmodified.
 */
static bool parse_color(const char *color, uint32_t *result) {
  if (color[0] == '#') {
    ++color;
  }

  int len = strlen(color);
  if (len != 6) {
    return false;
  }
  for (int i = 0; i < len; ++i) {
    if (!isxdigit(color[i])) {
      return false;
    }
  }

  uint32_t val = (uint32_t)strtoul(color, NULL, 16);
  *result = (val << 8) | 0xFF;
  return true;
}

struct swaybg_state {
  struct wl_display *display;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct wp_viewporter *viewporter;
  struct wp_single_pixel_buffer_manager_v1 *single_pixel_buffer_manager;
  struct wp_fractional_scale_manager_v1 *fract_scale_manager;
  struct wl_list configs; // struct swaybg_output_config::link
  struct wl_list outputs; // struct swaybg_output::link
  struct wl_list images;  // struct swaybg_image::link
  bool run_display;
  struct timespec start_time; // Track program start time for shaders
  struct wl_seat *seat;
  struct wl_pointer *pointer;
  unsigned int frame_rate;
  long long unsigned int frame_time;

  // Track mouse positions globally
  struct {
    float x, y;             // mouse position
    float down_x, down_y;   // position during button down
    float click_x, click_y; // position during last click
    bool is_down;           // button is down
    bool is_clicked;        // button was clicked
  } mouse;
};

struct swaybg_image {
  struct wl_list link;
  const char *path;
  cairo_surface_t *surface; // Store loaded surface
};

struct swaybg_output_config {
  char *output;
  const char *image_path;
  struct swaybg_image *image;
  enum background_mode mode;
  uint32_t color;
  struct wl_list link;
  enum zwlr_layer_shell_v1_layer layer;
  const char *shader_path;
};

struct swaybg_output {
  uint32_t wl_name;
  struct wl_output *wl_output;
  char *name;
  char *identifier;

  struct swaybg_state *state;
  struct swaybg_output_config *config;

  struct wl_surface *surface;
  struct zwlr_layer_surface_v1 *layer_surface;
  struct wp_viewport *viewport;
  struct wp_fractional_scale_v1 *fract_scale;

  uint32_t width, height;
  int32_t scale;
  uint32_t pref_fract_scale;

  uint32_t configure_serial;
  bool dirty, needs_ack;
  // dimensions of the wl_buffer attached to the wl_surface
  uint32_t buffer_width, buffer_height;

  struct wl_list link;

  // Shader state
  struct shader_context *shader_ctx;
  cairo_surface_t *shader_surface;

  int32_t x, y; // Output position

  // Track mouse positions relative to output
  struct {
    float x, y;             // mouse position
    float down_x, down_y;   // position during button down
    float click_x, click_y; // position during last click
  } mouse;
};

// Add new listeners
static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  struct swaybg_state *s = data;
  s->mouse.x = wl_fixed_to_double(sx * 2);
  s->mouse.y = wl_fixed_to_double(sy * 2);

  if (s->mouse.is_down) {
    s->mouse.down_x = s->mouse.x;
    s->mouse.down_y = s->mouse.y;
  }
}

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy) {
  // Update state when pointer enters any surface
  pointer_handle_motion(data, pointer, serial, sx, sy);
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface) {}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time,
                                  uint32_t button, uint32_t state) {
  struct swaybg_state *s = data;

  if (button == BTN_LEFT) {
    s->mouse.is_down = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    if (s->mouse.is_down) {
      s->mouse.is_clicked = true;
      s->mouse.click_x = s->mouse.x;
      s->mouse.click_y = s->mouse.y;
      s->mouse.down_x = s->mouse.x;
      s->mouse.down_y = s->mouse.y;
    } else {
      s->mouse.is_clicked = false;
    }
  }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis,
                                wl_fixed_t value) {}

static void pointer_handle_frame(void *data, struct wl_pointer *pointer) {}

static void pointer_handle_axis_source(void *data, struct wl_pointer *pointer,
                                       uint32_t axis_source) {}

static void pointer_handle_axis_stop(void *data, struct wl_pointer *pointer,
                                     uint32_t time, uint32_t axis) {}

static void pointer_handle_axis_discrete(void *data,
                                         struct wl_pointer *wl_pointer,
                                         uint32_t axis, int discrete) {}

static void pointer_handle_axis_value120(void *data,
                                         struct wl_pointer *wl_pointer,
                                         uint32_t axis, int value) {}

static void pointer_handle_axis_relative_direction(
    void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
    pointer_handle_frame,
    pointer_handle_axis_source,
    pointer_handle_axis_stop,
    pointer_handle_axis_discrete,
    pointer_handle_axis_value120,
    pointer_handle_axis_relative_direction,
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps) {
  struct swaybg_state *state = data;
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !state->pointer) {
    state->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(state->pointer, &pointer_listener, state);
  }
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
                             const char *name) {}

static const struct wl_seat_listener seat_listener = {seat_handle_capabilities,
                                                      seat_handle_name};

// Normalize and set mouse positions relative to output x and y
static void normalize_mouse_position(float input_mouse_x, float input_mouse_y,
                                     float *output_mouse_x,
                                     float *output_mouse_y, int32_t output_x,
                                     int32_t output_y) {
  if (input_mouse_x >= 0 && input_mouse_y >= 0) { // Valid position
    *output_mouse_x = input_mouse_x - output_x;
    *output_mouse_y = input_mouse_y - output_y;
  } else {
    // Use invalid position when mouse not available
    *output_mouse_x = -1;
    *output_mouse_y = -1;
  }
}

// Update output mouse positions before rendering
static void update_mouse_positions(struct swaybg_state *state) {
  struct swaybg_output *output;
  wl_list_for_each(output, &state->outputs, link) {
    normalize_mouse_position(state->mouse.x, state->mouse.y, &output->mouse.x,
                             &output->mouse.y, output->x, output->y);
    normalize_mouse_position(state->mouse.down_x, state->mouse.down_y,
                             &output->mouse.down_x, &output->mouse.down_y,
                             output->x, output->y);
    normalize_mouse_position(state->mouse.click_x, state->mouse.click_y,
                             &output->mouse.click_x, &output->mouse.click_y,
                             output->x, output->y);
  }
}

// Create a wl_buffer with the specified dimensions and content
static struct wl_buffer *draw_buffer(const struct swaybg_output *output,
                                     cairo_surface_t *surface,
                                     uint32_t buffer_width,
                                     uint32_t buffer_height) {
  uint32_t bg_color =
      output->config->color ? output->config->color : 0x000000ff;

  if (buffer_width == 1 && buffer_height == 1 &&
      output->config->mode == BACKGROUND_MODE_SOLID_COLOR &&
      output->state->single_pixel_buffer_manager) {
    // create and return single pixel buffer
    uint8_t r8 = (bg_color >> 24) & 0xFF;
    uint8_t g8 = (bg_color >> 16) & 0xFF;
    uint8_t b8 = (bg_color >> 8) & 0xFF;
    uint32_t r32 = (r8 << 24) | (r8 << 16) | (r8 << 8) | r8;
    uint32_t g32 = (g8 << 24) | (g8 << 16) | (g8 << 8) | g8;
    uint32_t b32 = (b8 << 24) | (b8 << 16) | (b8 << 8) | b8;
    return wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
        output->state->single_pixel_buffer_manager, r32, g32, b32, 0xFFFFFFFF);
  }

  struct pool_buffer buffer;
  if (!create_buffer(&buffer, output->state->shm, buffer_width, buffer_height,
                     WL_SHM_FORMAT_XRGB8888)) {
    return NULL;
  }

  cairo_t *cairo = buffer.cairo;
  cairo_set_source_u32(cairo, bg_color);
  cairo_paint(cairo);

  if (surface) {
    render_background_image(cairo, surface, output->config->mode, buffer_width,
                            buffer_height);
  }

  // return wl_buffer for caller to use and destroy
  struct wl_buffer *wl_buf = buffer.buffer;
  buffer.buffer = NULL;
  destroy_buffer(&buffer);
  return wl_buf;
}

#define FRACT_DENOM 120

// Return the size of the buffer that should be attached to this output
static void get_buffer_size(const struct swaybg_output *output,
                            uint32_t *buffer_width, uint32_t *buffer_height) {
  if (output->config->mode == BACKGROUND_MODE_SOLID_COLOR &&
      output->state->viewporter) {
    *buffer_width = 1;
    *buffer_height = 1;
  } else if (output->pref_fract_scale && output->state->viewporter) {
    // rounding mode is 'round half up'
    *buffer_width =
        (output->width * output->pref_fract_scale + FRACT_DENOM / 2) /
        FRACT_DENOM;
    *buffer_height =
        (output->height * output->pref_fract_scale + FRACT_DENOM / 2) /
        FRACT_DENOM;
  } else {
    *buffer_width = output->width * output->scale;
    *buffer_height = output->height * output->scale;
  }
}

static void render_frame(struct swaybg_output *output) {
  uint32_t buffer_width, buffer_height;
  get_buffer_size(output, &buffer_width, &buffer_height);

  // Calculate elapsed time for shaders
  double time = 0.0f;
  if (output->config->shader_path) {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    long seconds = now.tv_sec - output->state->start_time.tv_sec;
    long nanoseconds = now.tv_nsec - output->state->start_time.tv_nsec;
    time = (seconds % 86400) + nanoseconds / 1E9;
  }

  // Process shader if needed
  cairo_surface_t *render_surface = NULL;
  if (output->config->shader_path && output->config->image &&
      output->config->image->surface) {
    if (!output->shader_ctx) {
      output->shader_ctx = shader_context_create(
          output->config->shader_path,
          cairo_image_surface_get_width(output->config->image->surface),
          cairo_image_surface_get_height(output->config->image->surface));
    }

    if (output->shader_ctx) {
      if (!output->shader_surface ||
          cairo_image_surface_get_width(output->shader_surface) !=
              (int)buffer_width ||
          cairo_image_surface_get_height(output->shader_surface) !=
              (int)buffer_height) {
        if (output->shader_surface) {
          cairo_surface_destroy(output->shader_surface);
        }
        output->shader_surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, buffer_width, buffer_height);
      }

      shader_render(output->shader_ctx, output->config->image->surface,
                    output->shader_surface, time, output->mouse.x,
                    output->mouse.y, output->mouse.down_x, output->mouse.down_y,
                    output->mouse.click_x, output->mouse.click_y,
                    output->state->mouse.is_down,
                    output->state->mouse.is_clicked);
      render_surface = output->shader_surface;

      output->dirty = true;
    }
  } else if (output->config->image && output->config->image->surface) {
    render_surface = output->config->image->surface;
  }

  struct wl_buffer *buf = NULL;
  if (buffer_width != output->buffer_width ||
      buffer_height != output->buffer_height || output->dirty) {
    buf = draw_buffer(output, render_surface, buffer_width, buffer_height);
    if (!buf) {
      return;
    }

    wl_surface_attach(output->surface, buf, 0, 0);
    wl_surface_damage_buffer(output->surface, 0, 0, buffer_width,
                             buffer_height);
    output->buffer_width = buffer_width;
    output->buffer_height = buffer_height;
  }

  if (output->viewport) {
    wp_viewport_set_destination(output->viewport, output->width,
                                output->height);
  } else {
    wl_surface_set_buffer_scale(output->surface, output->scale);
  }

  wl_surface_commit(output->surface);
  if (buf) {
    wl_buffer_destroy(buf);
  }
}

static void destroy_swaybg_image(struct swaybg_image *image) {
  if (!image) {
    return;
  }
  wl_list_remove(&image->link);
  free(image);
}

static void destroy_swaybg_output_config(struct swaybg_output_config *config) {
  if (!config) {
    return;
  }
  wl_list_remove(&config->link);
  free(config->output);
  free(config);
}

static void destroy_swaybg_output(struct swaybg_output *output) {
  if (!output) {
    return;
  }

  if (output->state->pointer) {
    wl_pointer_release(output->state->pointer);
    output->state->pointer = NULL;
  }

  if (output->shader_ctx) {
    shader_context_destroy(output->shader_ctx);
  }

  if (output->shader_surface) {
    cairo_surface_destroy(output->shader_surface);
  }

  wl_list_remove(&output->link);
  if (output->layer_surface != NULL) {
    zwlr_layer_surface_v1_destroy(output->layer_surface);
  }
  if (output->surface != NULL) {
    wl_surface_destroy(output->surface);
  }
  if (output->viewport != NULL) {
    wp_viewport_destroy(output->viewport);
  }
  if (output->fract_scale != NULL) {
    wp_fractional_scale_v1_destroy(output->fract_scale);
  }
  wl_output_destroy(output->wl_output);
  free(output->name);
  free(output->identifier);
  free(output);
}

static void layer_surface_configure(void *data,
                                    struct zwlr_layer_surface_v1 *surface,
                                    uint32_t serial, uint32_t width,
                                    uint32_t height) {
  struct swaybg_output *output = data;
  output->width = width;
  output->height = height;
  output->dirty = true;
  output->configure_serial = serial;
  output->needs_ack = true;
}

static void layer_surface_closed(void *data,
                                 struct zwlr_layer_surface_v1 *surface) {
  struct swaybg_output *output = data;
  swaybg_log(LOG_DEBUG, "Destroying output %s (%s)", output->name,
             output->identifier);
  destroy_swaybg_output(output);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void fract_preferred_scale(void *data, struct wp_fractional_scale_v1 *f,
                                  uint32_t scale) {
  struct swaybg_output *output = data;
  output->pref_fract_scale = scale;
}

static const struct wp_fractional_scale_v1_listener fract_scale_listener = {
    .preferred_scale = fract_preferred_scale};

static void output_geometry(void *data, struct wl_output *output, int32_t x,
                            int32_t y, int32_t width_mm, int32_t height_mm,
                            int32_t subpixel, const char *make,
                            const char *model, int32_t transform) {
  struct swaybg_output *swaybg_output = data;
  swaybg_output->x = x;
  swaybg_output->y = y;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
  // Who cares
}

static void create_layer_surface(struct swaybg_output *output) {
  output->surface = wl_compositor_create_surface(output->state->compositor);
  assert(output->surface);

  if (output->state->fract_scale_manager) {
    output->fract_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
        output->state->fract_scale_manager, output->surface);
    assert(output->fract_scale);
    wp_fractional_scale_v1_add_listener(output->fract_scale,
                                        &fract_scale_listener, output);
  }

  if (output->state->viewporter &&
      (output->config->mode == BACKGROUND_MODE_SOLID_COLOR ||
       output->state->fract_scale_manager)) {
    output->viewport =
        wp_viewporter_get_viewport(output->state->viewporter, output->surface);
  }

  output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      output->state->layer_shell, output->surface, output->wl_output,
      output->config->layer, "wallpaper"); // Use config->layer here

  assert(output->layer_surface);

  zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
  zwlr_layer_surface_v1_set_anchor(output->layer_surface,
                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                       ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
  zwlr_layer_surface_v1_add_listener(output->layer_surface,
                                     &layer_surface_listener, output);
  wl_surface_commit(output->surface);
}

static void output_done(void *data, struct wl_output *wl_output) {
  struct swaybg_output *output = data;
  if (!output->config) {
    swaybg_log(LOG_DEBUG, "Could not find config for output %s (%s)",
               output->name, output->identifier);
    destroy_swaybg_output(output);
  } else if (!output->layer_surface) {
    swaybg_log(LOG_DEBUG, "Found config %s for output %s (%s)",
               output->config->output, output->name, output->identifier);
    create_layer_surface(output);
  }
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t scale) {
  struct swaybg_output *output = data;
  output->scale = scale;
  if (output->state->run_display && output->width > 0 && output->height > 0) {
    output->dirty = true;
  }
}

static void find_config(struct swaybg_output *output, const char *name) {
  struct swaybg_output_config *config = NULL;
  wl_list_for_each(config, &output->state->configs, link) {
    if (strcmp(config->output, name) == 0) {
      output->config = config;
      return;
    } else if (!output->config && strcmp(config->output, "*") == 0) {
      output->config = config;
    }
  }
}

static void output_name(void *data, struct wl_output *wl_output,
                        const char *name) {
  struct swaybg_output *output = data;
  output->name = strdup(name);

  // If description was sent first, the config may already be populated. If
  // there is an identifier config set, keep it.
  if (!output->config || strcmp(output->config->output, "*") == 0) {
    find_config(output, name);
  }
}

static void output_description(void *data, struct wl_output *wl_output,
                               const char *description) {
  struct swaybg_output *output = data;

  // wlroots currently sets the description to `make model serial (name)`
  // If this changes in the future, this will need to be modified.
  char *paren = strrchr(description, '(');
  if (paren) {
    size_t length = paren - description;
    output->identifier = malloc(length);
    if (!output->identifier) {
      swaybg_log(LOG_ERROR, "Failed to allocate output identifier");
      return;
    }
    strncpy(output->identifier, description, length);
    output->identifier[length - 1] = '\0';

    find_config(output, output->identifier);
  }
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
  struct swaybg_state *state = data;
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    struct swaybg_output *output = calloc(1, sizeof(struct swaybg_output));
    output->state = state;
    output->scale = 1;
    output->wl_name = name;
    output->wl_output =
        wl_registry_bind(registry, name, &wl_output_interface, 4);
    wl_output_add_listener(output->wl_output, &output_listener, output);
    wl_list_insert(&state->outputs, &output->link);

    output->x = 0;
    output->y = 0;
    // Initialize output mouse positions to invalid positions
    output->mouse.x = -1;
    output->mouse.y = -1;
    output->mouse.down_x = -1;
    output->mouse.down_y = -1;
    output->mouse.click_x = -1;
    output->mouse.click_y = -1;
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    state->layer_shell =
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
    state->viewporter =
        wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
  } else if (strcmp(interface,
                    wp_single_pixel_buffer_manager_v1_interface.name) == 0) {
    state->single_pixel_buffer_manager = wl_registry_bind(
        registry, name, &wp_single_pixel_buffer_manager_v1_interface, 1);
  } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) ==
             0) {
    state->fract_scale_manager = wl_registry_bind(
        registry, name, &wp_fractional_scale_manager_v1_interface, 1);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
    wl_seat_add_listener(state->seat, &seat_listener, state);
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                                 uint32_t name) {
  struct swaybg_state *state = data;
  struct swaybg_output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &state->outputs, link) {
    if (output->wl_name == name) {
      swaybg_log(LOG_DEBUG, "Destroying output %s (%s)", output->name,
                 output->identifier);
      destroy_swaybg_output(output);
      break;
    }
  }
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static bool store_swaybg_output_config(struct swaybg_state *state,
                                       struct swaybg_output_config *config) {
  struct swaybg_output_config *oc = NULL;
  wl_list_for_each(oc, &state->configs, link) {
    if (strcmp(config->output, oc->output) == 0) {
      // Merge on top
      if (config->image_path) {
        oc->image_path = config->image_path;
      }
      if (config->color) {
        oc->color = config->color;
      }
      if (config->mode != BACKGROUND_MODE_INVALID) {
        oc->mode = config->mode;
      }
      return false;
    }
  }
  // New config, just add it
  wl_list_insert(&state->configs, &config->link);
  return true;
}

static void parse_command_line(int argc, char **argv,
                               struct swaybg_state *state) {
  static struct option long_options[] = {
      {"color", required_argument, NULL, 'c'},
      {"fps", required_argument, NULL, 'f'},
      {"help", no_argument, NULL, 'h'},
      {"image", required_argument, NULL, 'i'},
      {"layer", required_argument, NULL, 'l'},
      {"mode", required_argument, NULL, 'm'},
      {"output", required_argument, NULL, 'o'},
      {"shader", required_argument, NULL, 's'},
      {"version", no_argument, NULL, 'v'},
      {0, 0, 0, 0}};

  const char *usage =
      "Usage: swaybg <options...>\n"
      "\n"
      "  -c, --color RRGGBB     Set the background color.\n"
      "  -f, --fps <int>        FPS of the shader.\n"
      "  -h, --help             Show help message and quit.\n"
      "  -i, --image <path>     Set the image to display.\n"
      "  -l, --layer <layer>    Set the layer to use: background, bottom, "
      "top, overlay\n"
      "  -m, --mode <mode>      Set the mode to use for the image.\n"
      "  -o, --output <name>    Set the output to operate on or * for all.\n"
      "  -s, --shader <path>    Apply GLSL shader to wallpaper\n"
      "  -v, --version          Show the version number and quit.\n"
      "\n"
      "Background Modes:\n"
      "  stretch, fit, fill, center, tile, or solid_color\n";

  struct swaybg_output_config *config =
      calloc(1, sizeof(struct swaybg_output_config));
  config->output = strdup("*");
  config->mode = BACKGROUND_MODE_INVALID;
  wl_list_init(&config->link); // init for safe removal

  int c;
  while (1) {
    int option_index = 0;
    c = getopt_long(argc, argv, "c:f:hi:l:m:o:s:v", long_options,
                    &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'c': // color
      if (!parse_color(optarg, &config->color)) {
        swaybg_log(LOG_ERROR,
                   "%s is not a valid color for swaybg. "
                   "Color should be specified as rrggbb or #rrggbb (no alpha).",
                   optarg);
        continue;
      }
      break;
    case 'f':
      state->frame_rate = atoi(optarg);
      if (state->frame_rate == 0) {
        state->frame_rate = 60; // Default 60 fps
      }
      state->frame_time = 1.0 / state->frame_rate * 1E9;
      break;
    case 'i': // image
      config->image_path = optarg;
      break;
    case 'm': // mode
      config->mode = parse_background_mode(optarg);
      if (config->mode == BACKGROUND_MODE_INVALID) {
        swaybg_log(LOG_ERROR, "Invalid mode: %s", optarg);
      }
      break;
    case 'o': // output
      if (config && !store_swaybg_output_config(state, config)) {
        // Empty config or merged on top of an existing one
        destroy_swaybg_output_config(config);
      }
      config = calloc(1, sizeof(struct swaybg_output_config));
      config->output = strdup(optarg);
      config->mode = BACKGROUND_MODE_INVALID;
      wl_list_init(&config->link); // init for safe removal
      break;
    case 'v': // version
      fprintf(stdout, "swaybg version " SWAYBG_VERSION "\n");
      exit(EXIT_SUCCESS);
      break;
    case 'l': // layer
      if (strcmp(optarg, "background") == 0) {
        config->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
      } else if (strcmp(optarg, "bottom") == 0) {
        config->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
      } else if (strcmp(optarg, "top") == 0) {
        config->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
      } else if (strcmp(optarg, "overlay") == 0) {
        config->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
      } else {
        swaybg_log(LOG_ERROR, "Invalid layer: %s", optarg);
      }
      break;
    case 's': // shader
      config->shader_path = optarg;
      break;
    default:
      fprintf(c == 'h' ? stdout : stderr, "%s", usage);
      exit(c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
      break;
    }
  }
  if (config && !store_swaybg_output_config(state, config)) {
    // Empty config or merged on top of an existing one
    destroy_swaybg_output_config(config);
  }

  // Check for invalid options
  if (optind < argc) {
    config = NULL;
    struct swaybg_output_config *tmp = NULL;
    wl_list_for_each_safe(config, tmp, &state->configs, link) {
      destroy_swaybg_output_config(config);
    }
    // continue into empty list
  }
  if (wl_list_empty(&state->configs)) {
    fprintf(stderr, "%s", usage);
    exit(EXIT_FAILURE);
  }

  // Set default mode and remove empties
  config = NULL;
  struct swaybg_output_config *tmp = NULL;
  wl_list_for_each_safe(config, tmp, &state->configs, link) {
    if (!config->image_path && !config->color) {
      destroy_swaybg_output_config(config);
    } else if (config->mode == BACKGROUND_MODE_INVALID) {
      config->mode = config->image_path ? BACKGROUND_MODE_STRETCH
                                        : BACKGROUND_MODE_SOLID_COLOR;
    }
  }
}

int main(int argc, char **argv) {
  swaybg_log_init(LOG_DEBUG);

  struct swaybg_state state = {0};
  wl_list_init(&state.configs);
  wl_list_init(&state.outputs);
  wl_list_init(&state.images);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                &state.start_time); // Initialize shader time

  // Initialize frame rate
  state.frame_rate = 60;
  state.frame_time = 1.0 / state.frame_rate * 1E9;

  // Initialize mouse positions to invalid position
  state.mouse.x = -1;
  state.mouse.y = -1;
  state.mouse.down_x = -1;
  state.mouse.down_y = -1;
  state.mouse.click_x = -1;
  state.mouse.click_y = -1;
  state.mouse.is_down = false;
  state.mouse.is_clicked = false;

  parse_command_line(argc, argv, &state);

  printf("Frame rate: %u\n", state.frame_rate);
  printf("Frame time: %llu\n", state.frame_time);

  // Load images
  struct swaybg_image *image;
  struct swaybg_output_config *config;
  wl_list_for_each(config, &state.configs, link) {
    if (!config->image_path)
      continue;

    bool found = false;
    wl_list_for_each(image, &state.images, link) {
      if (strcmp(image->path, config->image_path) == 0) {
        config->image = image;
        found = true;
        break;
      }
    }

    if (!found) {
      image = calloc(1, sizeof(struct swaybg_image));
      image->path = config->image_path;
      image->surface = load_background_image(image->path);
      if (!image->surface) {
        swaybg_log(LOG_ERROR, "Failed to load image: %s", image->path);
        free(image);
        continue;
      }
      wl_list_insert(&state.images, &image->link);
      config->image = image;
    }
  }

  state.display = wl_display_connect(NULL);
  if (!state.display) {
    swaybg_log(LOG_ERROR, "Unable to connect to the compositor. "
                          "If your compositor is running, check or set the "
                          "WAYLAND_DISPLAY environment variable.");
    return 1;
  }

  struct wl_registry *registry = wl_display_get_registry(state.display);
  wl_registry_add_listener(registry, &registry_listener, &state);
  if (wl_display_roundtrip(state.display) < 0) {
    swaybg_log(LOG_ERROR, "wl_display_roundtrip failed");
    return 1;
  }
  if (state.compositor == NULL || state.shm == NULL ||
      state.layer_shell == NULL) {
    swaybg_log(LOG_ERROR, "Missing a required Wayland interface");
    return 1;
  }

  state.run_display = true;
  while (wl_display_dispatch(state.display) != -1 && state.run_display) {
    update_mouse_positions(&state);

    // Send acks, and determine which images need to be loaded
    struct swaybg_output *output;
    wl_list_for_each(output, &state.outputs, link) {
      if (output->needs_ack) {
        output->needs_ack = false;
        zwlr_layer_surface_v1_ack_configure(output->layer_surface,
                                            output->configure_serial);
      }
    }

    wl_list_for_each(output, &state.outputs, link) {
      if (output->dirty) {
        output->dirty = false;
        render_frame(output);
      }
    }

    // Update mouse state (is clicked should only be for one frame)
    state.mouse.is_clicked = false;

    // Limit to 60 FPS
    struct timespec sleep_time = {.tv_nsec = state.frame_time};
    nanosleep(&sleep_time, NULL);
  }

  struct swaybg_output *output, *tmp_output;
  wl_list_for_each_safe(output, tmp_output, &state.outputs, link) {
    destroy_swaybg_output(output);
  }

  struct swaybg_output_config *tmp_config = NULL;
  wl_list_for_each_safe(config, tmp_config, &state.configs, link) {
    destroy_swaybg_output_config(config);
  }

  struct swaybg_image *tmp_image;
  wl_list_for_each_safe(image, tmp_image, &state.images, link) {
    cairo_surface_destroy(image->surface);
    destroy_swaybg_image(image);
  }

  if (state.seat) {
    wl_seat_release(state.seat);
  }

  if (state.display) {
    wl_display_disconnect(state.display);
  }

  return 0;
}
