#include "shader.h"
#include "shader_uniform.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <errno.h>
#include <getopt.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// clang-format off
#define USAGE_STRING                                                            \
  "Usage: wlsbg <options...> [OUTPUT] SHADER.frag\n"                            \
  "\n"                                                                          \
	"Options:\n"																																  \
  "  -h,     --help                   Show help message and quit.\n"            \
  "  -v,     --version                Show the version number and quit.\n"      \
  "  -f,     --fps <int>              Max FPS limit of the shader.\n"           \
  "  -l,     --layer <layer>          Set the layer to display on.\n"           \
  "  -s,     --shared-shader <path>   Set the shared shader file.\n"            \
  "  -[0-9], --channel[0-9] <path>    Set the resource for a channel.\n"        \
  "\n"                                                                          \
	"Required Arguments:\n"																											  \
  "  [OUTPUT] - Set the output to operate on or '*' for all.\n"								  \
	"\n"																																				  \
  "Layer Types:\n"                                                              \
  "  background, bottom, top, or overlay\n"                                     \
	"\n"																																				  \
  "Channel Resources (More information can be found in the man page):\n"        \
  "	- `t:<path>`: Load texture from image file\n"                               \
  "	- `b:<path>`: Create shader buffer from fragment shader\n"                  \
  "	- `(resources...)`: Nested buffer definitions\n"                            \
  "	- `<T>Name:<path>`: Named resources, parsed from left to right\n"
// clang-format on

#define DEFAULT_FPS 60

static const struct option options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"fps", required_argument, NULL, 'f'},
    {"layer", required_argument, NULL, 'l'},
    {"shared-shader", required_argument, NULL, 's'},
    {"channel0", required_argument, NULL, '0'},
    {"channel1", required_argument, NULL, '1'},
    {"channel2", required_argument, NULL, '2'},
    {"channel3", required_argument, NULL, '3'},
    {"channel4", required_argument, NULL, '4'},
    {"channel5", required_argument, NULL, '5'},
    {"channel6", required_argument, NULL, '6'},
    {"channel7", required_argument, NULL, '7'},
    {"channel8", required_argument, NULL, '8'},
    {"channel9", required_argument, NULL, '9'},
    {0, 0, 0, 0}};

struct state {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct zwlr_layer_shell_v1 *layer_shell;
  struct wl_seat *seat;
  struct wl_pointer *pointer;
  struct wl_list outputs;

  char *output_name;
  char *shader_path;
  char *shared_shader_path;
  float fps;
  enum zwlr_layer_shell_v1_layer layer;
  struct timespec start_time;

  char *channel_input[10];

  // Track mouse positions
  struct {
    float x, y;             // mouse position
    float down_x, down_y;   // position during button down
    float click_x, click_y; // position during last click
    bool is_down;           // button is down
    bool is_clicked;        // button was clicked
  } mouse;
};

struct output {
  struct wl_list link;
  struct state *state;
  uint32_t wl_name;
  struct wl_output *wl_output;
  char *name;

  struct wl_surface *surface;
  struct zwlr_layer_surface_v1 *layer_surface;
  shader_context *shader_ctx;

  int width, height;
  bool needs_ack;
  bool needs_resize;
  uint32_t last_serial;
  struct wl_callback *frame_callback;
};

static void destroy_output(struct output *output) {
  if (!output)
    return;

  if (output->shader_ctx)
    shader_destroy(output->shader_ctx);
  if (output->frame_callback)
    wl_callback_destroy(output->frame_callback);
  if (output->surface)
    wl_surface_destroy(output->surface);
  if (output->layer_surface)
    zwlr_layer_surface_v1_destroy(output->layer_surface);
  wl_list_remove(&output->link);
  if (output->wl_output)
    wl_output_destroy(output->wl_output);
  free(output->name);
  free(output);
}

// <{{ Frame callback listener

static void frame_done(void *data, struct wl_callback *wl_callback,
                       uint32_t callback_data) {
  struct output *output = data;
  if (!output || output->frame_callback != wl_callback) {
    fprintf(stderr, "Frame callback tracking error\n");
    exit(EXIT_FAILURE);
  }
  /* clearing frame callback field indicates output is ready for drawing.
   * Note that this callback will interrupt the call to poll in the main
   * loop.
   */
  wl_callback_destroy(wl_callback);
  output->frame_callback = NULL;
}

static const struct wl_callback_listener frame_callback_listener = {
    .done = frame_done};

// }}>

// <{{ Layer surface listener

static void layer_surface_configure(void *data,
                                    struct zwlr_layer_surface_v1 *surface,
                                    uint32_t serial, uint32_t width,
                                    uint32_t height) {
  struct output *output = data;
  if (!output)
    return;
  struct state *state = output->state;

  if (width > 0)
    output->width = width;
  if (height > 0)
    output->height = height;

  if (!output->shader_ctx) {
    // First configure: create shader context
    output->shader_ctx =
        shader_create(state->display, output->surface, state->shader_path,
                      state->shared_shader_path, output->width, output->height,
                      state->channel_input);
    if (!output->shader_ctx) {
      fprintf(stderr, "Failed to create shader context\n");
      exit(EXIT_FAILURE);
    }

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    // First draw
    shader_render(output->shader_ctx, 0, NULL);

    // Setup frame callback (rendering happens later)
    output->frame_callback = wl_surface_frame(output->surface);
    wl_callback_add_listener(output->frame_callback, &frame_callback_listener,
                             output);

    output->needs_resize = false; // Already created with correct size
  } else {
    // Always set ACK flag for every configure
    output->needs_ack = true;
    output->last_serial = serial;
    output->needs_resize = true; // Subsequent configures need resize
  }
}

static void layer_surface_closed(void *data,
                                 struct zwlr_layer_surface_v1 *surface) {
  struct output *output = data;
  if (output)
    destroy_output(output);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure, .closed = layer_surface_closed};

// }}>

// <{{ Output listener

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x,
                            int32_t y, int32_t width_mm, int32_t height_mm,
                            int32_t subpixel, const char *make,
                            const char *model, int32_t transform) {
  // Unused
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
  // Unused
}

static void output_done(void *data, struct wl_output *wl_output) {
  struct output *output = data;
  if (!output)
    return;
  if (output->surface) {
    /* output has already been given a surface */
    return;
  }

  struct state *state = output->state;
  bool wildcard = state->output_name && !strcmp(state->output_name, "*");
  if (wildcard || (output->name && state->output_name &&
                   !strcmp(output->name, state->output_name))) {
    output->surface = wl_compositor_create_surface(state->compositor);
    if (!output->surface) {
      fprintf(stderr, "Failed to create surface\n");
      return;
    }
    output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, output->surface, output->wl_output, state->layer,
        "wlsbg");
    if (!output->layer_surface) {
      fprintf(stderr, "Failed to create layer surface\n");
      wl_surface_destroy(output->surface);
      output->surface = NULL;
      return;
    }

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
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor) {
  // Unused
}

static void output_name(void *data, struct wl_output *wl_output,
                        const char *name) {
  struct output *output = data;
  if (!output)
    return;
  free(output->name);
  output->name = name ? strdup(name) : NULL;
}

static void output_description(void *data, struct wl_output *wl_output,
                               const char *description) {
  // Unused
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description};

// }}>

// <{{ Mouse listener

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  struct state *s = data;
  if (!s)
    return;
  s->mouse.x = wl_fixed_to_double(sx);
  s->mouse.y = wl_fixed_to_double(sy);

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
  struct state *s = data;
  if (!s)
    return;

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

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
};
// }}>

// <{{ Seat listener

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps) {
  struct state *state = data;
  if (!state)
    return;
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !state->pointer) {
    state->pointer = wl_seat_get_pointer(seat);
    if (state->pointer) {
      wl_pointer_add_listener(state->pointer, &pointer_listener, state);
    }
  }
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
                             const char *name) {}

static const struct wl_seat_listener seat_listener = {seat_handle_capabilities,
                                                      seat_handle_name};

// }}>

// <{{ Registry listener

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  struct state *state = data;
  if (!state)
    return;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 1);
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    state->layer_shell =
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, wl_output_interface.name) == 0 && version >= 4) {
    // Only accept version >= 4 outputs, as those provide their name/description
    struct output *output = calloc(1, sizeof(struct output));
    if (!output)
      return;
    output->state = state;
    output->wl_name = name;
    output->wl_output =
        wl_registry_bind(registry, name, &wl_output_interface, 4);
    if (!output->wl_output) {
      free(output);
      return;
    }
    wl_output_add_listener(output->wl_output, &output_listener, output);
    wl_list_insert(&state->outputs, &output->link);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    if (state->seat) {
      wl_seat_add_listener(state->seat, &seat_listener, state);
    }
  }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name) {
  struct state *state = data;
  if (!state)
    return;
  struct output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &state->outputs, link) {
    if (output->wl_name == name) {
      destroy_output(output);
    }
  }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

// }}>

static double timespec_to_sec(struct timespec ts) {
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
  struct state state = {0};
  state.fps = DEFAULT_FPS;
  state.layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
  wl_list_init(&state.outputs);
  clock_gettime(CLOCK_MONOTONIC, &state.start_time);

  // Parse command line
  int opt;
  while ((opt = getopt_long(argc, argv, "hvf:l:s:0:1:2:3:4:5:6:7:8:9:", options,
                            NULL)) != -1) {
    switch (opt) {
    case 'h':
      printf(USAGE_STRING);
      return EXIT_SUCCESS;
    case 'v':
      printf("wlsbg version " WLSBG_VERSION "\n");
      return EXIT_SUCCESS;
    case 'f':
      state.fps = atof(optarg);
      if (state.fps <= 0) {
        state.fps = DEFAULT_FPS;
        fprintf(stderr, "FPS must be a valid number >0, defaulting to 60fps\n");
      }
      break;
    case 'l':
      if (strcmp(optarg, "background") == 0) {
        state.layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
      } else if (strcmp(optarg, "bottom") == 0) {
        state.layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
      } else if (strcmp(optarg, "top") == 0) {
        state.layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
      } else if (strcmp(optarg, "overlay") == 0) {
        state.layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
      }
      break;
    case 's':
      state.shared_shader_path = optarg;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      unsigned char channel = opt - '0';
      state.channel_input[channel] = optarg;
      break;
    }
    default:
      fprintf(stderr, USAGE_STRING);
      return EXIT_FAILURE;
    }
  }

  if (argc - optind < 2) {
    fprintf(stderr, USAGE_STRING);
    return EXIT_FAILURE;
  }

  state.output_name = argv[optind];
  state.shader_path = argv[optind + 1];

  // Connect to Wayland
  state.display = wl_display_connect(NULL);
  if (!state.display) {
    fprintf(stderr, "Failed to connect to Wayland display\n");
    return EXIT_FAILURE;
  }

  state.registry = wl_display_get_registry(state.display);
  if (!state.registry) {
    fprintf(stderr, "Failed to get registry\n");
    wl_display_disconnect(state.display);
    return EXIT_FAILURE;
  }
  wl_registry_add_listener(state.registry, &registry_listener, &state);

  for (int i = 0; i < 3; ++i) {
    if (wl_display_roundtrip(state.display) < 0) {
      fprintf(stderr, "wl_display_roundtrip failed");
      wl_registry_destroy(state.registry);
      wl_display_disconnect(state.display);
      return 1;
    }
  }

  if (!state.compositor || !state.layer_shell) {
    fprintf(stderr, "Missing required Wayland interfaces\n");
    if (state.registry)
      wl_registry_destroy(state.registry);
    if (state.display)
      wl_display_disconnect(state.display);
    return EXIT_FAILURE;
  }

  // Main loop
  int display_fd = wl_display_get_fd(state.display);
  double frame_time = 1.0 / state.fps;
  struct timespec next_frame = state.start_time;

  while (true) {
    if (wl_display_dispatch_pending(state.display) == -1) {
      fprintf(stderr, "Failed to dispatch events\n");
      break;
    }
    if (wl_display_flush(state.display) == -1 && errno != EAGAIN) {
      fprintf(stderr, "Failed to flush\n");
      break;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Calculate time until next frame
    double sec_until_next = timespec_to_sec(next_frame) - timespec_to_sec(now);
    int timeout_ms = sec_until_next > 0 ? (int)(sec_until_next * 1000) : 0;

    // Poll for events
    struct pollfd pfd = {display_fd, POLLIN, 0};
    int poll_result = poll(&pfd, 1, timeout_ms);

    if (poll_result < 0 && errno != EINTR) {
      perror("poll failed");
      break;
    }

    if (pfd.revents & POLLIN) {
      if (wl_display_prepare_read(state.display) == -1) {
        fprintf(stderr, "Failed to prepare read\n");
        break;
      }
      if (wl_display_read_events(state.display) == -1) {
        fprintf(stderr, "Failed to read events\n");
        break;
      }
    }

    // Render if it's time
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (timespec_to_sec(now) >= timespec_to_sec(next_frame)) {
      // Update next frame time
      next_frame.tv_nsec += (long)(frame_time * 1e9);
      if (next_frame.tv_nsec >= 1e9) {
        next_frame.tv_nsec -= 1e9;
        next_frame.tv_sec++;
      }

      // Calculate elapsed time
      double elapsed = timespec_to_sec(now) - timespec_to_sec(state.start_time);

      // Render all outputs
      struct output *output, *tmp;
      wl_list_for_each_safe(output, tmp, &state.outputs, link) {
        if (!output->shader_ctx || output->frame_callback)
          continue;

        // Handle pending resize
        if (output->needs_resize) {
          shader_resize(output->shader_ctx, output->width, output->height);
          output->needs_resize = false;
        }

        // Handle pending configure ack
        if (output->needs_ack) {
          zwlr_layer_surface_v1_ack_configure(output->layer_surface,
                                              output->last_serial);
          output->needs_ack = false;
        }

        // Create iMouse struct for uniform
        iMouse mouse = {.real_x = state.mouse.x,
                        .real_y = output->height - state.mouse.y,
                        .x = state.mouse.down_x,
                        .y = output->height - state.mouse.down_y,
                        .z = state.mouse.is_down ? state.mouse.click_x
                                                 : -state.mouse.click_x,
                        .w = state.mouse.is_clicked
                                 ? output->height - state.mouse.click_y
                                 : -(output->height - state.mouse.click_y)};
        // Mouse click should only be for 1 frame
        state.mouse.is_clicked = false;

        shader_render(output->shader_ctx, elapsed, &mouse);

        // Setup frame callback
        output->frame_callback = wl_surface_frame(output->surface);
        wl_callback_add_listener(output->frame_callback,
                                 &frame_callback_listener, output);
      }
    }
  }

  // Cleanup outputs
  struct output *output, *tmp;
  wl_list_for_each_safe(output, tmp, &state.outputs, link) {
    destroy_output(output);
  }

  if (state.pointer)
    wl_pointer_release(state.pointer);
  if (state.seat)
    wl_seat_destroy(state.seat);
  if (state.layer_shell)
    zwlr_layer_shell_v1_destroy(state.layer_shell);
  if (state.compositor)
    wl_compositor_destroy(state.compositor);
  if (state.registry)
    wl_registry_destroy(state.registry);
  if (state.display)
    wl_display_disconnect(state.display);

  return EXIT_SUCCESS;
}
