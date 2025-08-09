#include "shader_video.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static void *get_proc_address_mpv(void *ctx, const char *name) {
  (void)ctx;
  return eglGetProcAddress(name);
}

// Get current time in seconds
static double get_time_seconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

shader_video *shader_video_create(char *path) {
  if (!path) {
    fprintf(stderr, "Invalid path provided\n");
    return NULL;
  }

  shader_video *vid = calloc(1, sizeof(shader_video));
  if (!vid) {
    return NULL;
  }

  vid->path = path;
  if (!vid->path) {
    free(vid);
    return NULL;
  }

  vid->mpv = mpv_create();
  if (!vid->mpv) {
    fprintf(stderr, "Failed to create mpv context\n");
    free(vid);
    return NULL;
  }

  // Configure mpv for smooth playback
  mpv_set_option_string(vid->mpv, "vo", "libmpv");
  mpv_set_option_string(vid->mpv, "hwdec", "auto-safe");
  mpv_set_option_string(vid->mpv, "loop", "inf");
  mpv_set_option_string(vid->mpv, "audio", "no");
  mpv_set_option_string(vid->mpv, "video-sync", "display-resample");

  // Enable smooth seeking and frame dropping
  mpv_set_option_string(vid->mpv, "hr-seek", "yes");
  mpv_set_option_string(vid->mpv, "hr-seek-framedrop", "yes");
  mpv_set_option_string(vid->mpv, "cache", "yes");
  mpv_set_option_string(vid->mpv, "cache-secs", "10");

  // Start playing immediately instead of paused
  mpv_set_option_string(vid->mpv, "pause", "no");

  if (mpv_initialize(vid->mpv) < 0) {
    fprintf(stderr, "Failed to initialize mpv\n");
    mpv_destroy(vid->mpv);
    free(vid);
    return NULL;
  }

  // Create render context
  mpv_opengl_init_params gl_init = {
      .get_proc_address = get_proc_address_mpv,
  };

  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
      {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };

  if (mpv_render_context_create(&vid->mpv_gl, vid->mpv, params) < 0) {
    fprintf(stderr, "Failed to create mpv render context\n");
    mpv_destroy(vid->mpv);
    free(vid);
    return NULL;
  }

  // Load file
  const char *cmd[] = {"loadfile", path, NULL};
  if (mpv_command(vid->mpv, cmd) < 0) {
    fprintf(stderr, "Failed to load video file: %s\n", path);
    mpv_render_context_free(vid->mpv_gl);
    mpv_destroy(vid->mpv);
    free(vid);
    return NULL;
  }

  // Create texture and framebuffer for rendering
  glGenTextures(1, &vid->tex_id);
  if (vid->tex_id == 0) {
    fprintf(stderr, "Failed to generate texture\n");
    mpv_render_context_free(vid->mpv_gl);
    mpv_destroy(vid->mpv);
    free(vid);
    return NULL;
  }

  glBindTexture(GL_TEXTURE_2D, vid->tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create framebuffer for rendering video to texture
  glGenFramebuffers(1, &vid->fbo);
  if (vid->fbo == 0) {
    fprintf(stderr, "Failed to generate framebuffer\n");
    glDeleteTextures(1, &vid->tex_id);
    mpv_render_context_free(vid->mpv_gl);
    mpv_destroy(vid->mpv);
    free(vid);
    return NULL;
  }

  // Initialize timing
  vid->start_time = get_time_seconds();
  vid->last_seek_time = -1.0;
  vid->duration = 0.0;
  vid->seek_threshold = 0.3; // Only seek if off by more than 300ms
  vid->fbo_configured = false;
  vid->playing = true;

  return vid;
}

void shader_video_update(shader_video *vid, double current_time) {
  if (!vid || !vid->mpv)
    return;

  // Process mpv events (but limit to avoid blocking)
  int event_count = 0;
  mpv_event *event;
  while ((event = mpv_wait_event(vid->mpv, 0)) != NULL && event_count < 10) {
    if (event->event_id == MPV_EVENT_NONE)
      break;

    event_count++;

    switch (event->event_id) {
    case MPV_EVENT_VIDEO_RECONFIG: {
      int64_t width, height;
      if (mpv_get_property(vid->mpv, "video-params/w", MPV_FORMAT_INT64,
                           &width) >= 0 &&
          mpv_get_property(vid->mpv, "video-params/h", MPV_FORMAT_INT64,
                           &height) >= 0) {

        int new_width = (int)width;
        int new_height = (int)height;

        // Only update texture if size actually changed
        if (new_width != vid->width || new_height != vid->height) {
          vid->width = new_width;
          vid->height = new_height;

          // Update texture size
          glBindTexture(GL_TEXTURE_2D, vid->tex_id);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid->width, vid->height, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, NULL);

          vid->fbo_configured = false; // Mark for reconfiguration
        }
      }
      break;
    }
    case MPV_EVENT_SEEK:
      vid->seeking = false;
      break;
    case MPV_EVENT_FILE_LOADED: {
      // Get duration when file is loaded
      double duration;
      if (mpv_get_property(vid->mpv, "duration", MPV_FORMAT_DOUBLE,
                           &duration) >= 0) {
        vid->duration = duration;
      }
      break;
    }
    default:
      break;
    }
  }

  // Only seek if we're significantly out of sync and not already seeking
  if (!vid->seeking && vid->duration > 0.0) {
    double target_time = fmod(current_time, vid->duration);

    double current_pos;
    if (mpv_get_property(vid->mpv, "time-pos", MPV_FORMAT_DOUBLE,
                         &current_pos) >= 0) {
      double time_diff = fabs(target_time - current_pos);

      // Only seek if we're off by more than the threshold
      if (time_diff > vid->seek_threshold) {
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "%.3f", target_time);
        const char *seek_cmd[] = {"seek", time_str, "absolute+exact", NULL};

        if (mpv_command_async(vid->mpv, 0, seek_cmd) >= 0) {
          vid->seeking = true;
          vid->last_seek_time = target_time;
        }
      }
    }
  }
}

void shader_video_render(shader_video *vid) {
  if (!vid->mpv_gl || vid->width == 0 || vid->height == 0)
    return;

  // Configure framebuffer only once or when size changes
  if (!vid->fbo_configured) {
    glBindFramebuffer(GL_FRAMEBUFFER, vid->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           vid->tex_id, 0);

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Framebuffer not complete: %d\n", status);
      return;
    }

    vid->fbo_configured = true;
  } else {
    // Just bind the already configured framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, vid->fbo);
  }

  // Save current state
  GLint current_fbo, viewport[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
  glGetIntegerv(GL_VIEWPORT, viewport);

  // Set viewport for video rendering
  glViewport(0, 0, vid->width, vid->height);

  // Clear the framebuffer
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Render mpv frame to our texture
  mpv_opengl_fbo mpv_fbo = {.fbo = vid->fbo,
                            .w = vid->width,
                            .h = vid->height,
                            .internal_format = GL_RGBA8};

  int flip = 1;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };

  // Check if we need to render a new frame
  uint64_t flags = mpv_render_context_update(vid->mpv_gl);
  if (flags & MPV_RENDER_UPDATE_FRAME) {
    mpv_render_context_render(vid->mpv_gl, params);
  }

  // Restore previous state
  glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void shader_video_set_time(shader_video *vid, double time) {
  if (!vid || vid->duration <= 0.0)
    return;

  double target_time = fmod(time, vid->duration);
  char time_str[32];
  snprintf(time_str, sizeof(time_str), "%.3f", target_time);
  const char *seek_cmd[] = {"seek", time_str, "absolute+exact", NULL};
  mpv_command_async(vid->mpv, 0, seek_cmd);
}

void shader_video_destroy(shader_video *vid) {
  if (!vid)
    return;

  // Clean up OpenGL resources first
  if (vid->fbo) {
    glDeleteFramebuffers(1, &vid->fbo);
    vid->fbo = 0;
  }

  if (vid->tex_id) {
    glDeleteTextures(1, &vid->tex_id);
    vid->tex_id = 0;
  }

  // Clean up mpv resources
  if (vid->mpv_gl) {
    mpv_render_context_free(vid->mpv_gl);
    vid->mpv_gl = NULL;
  }

  if (vid->mpv) {
    mpv_destroy(vid->mpv);
    vid->mpv = NULL;
  }

  // Clean up path
  if (vid->path) {
    free(vid->path);
    vid->path = NULL;
  }

  free(vid);
}
