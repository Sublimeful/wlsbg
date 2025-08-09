#include "shader_video.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void *get_proc_address_mpv(void *ctx, const char *name) {
  (void)ctx;
  return eglGetProcAddress(name);
}

shader_video *shader_video_create(char *path) {
  shader_video *vid = calloc(1, sizeof(shader_video));
  if (!vid) {
    goto error;
  }

  vid->path = path;
  if (!vid->path) {
    goto error;
  }

  vid->mpv = mpv_create();
  if (!vid->mpv) {
    goto error;
  }

  // Configure mpv
  mpv_set_option_string(vid->mpv, "vo", "libmpv");
  mpv_set_option_string(vid->mpv, "hwdec", "auto-safe");
  mpv_set_option_string(vid->mpv, "loop", "inf");
  mpv_set_option_string(vid->mpv, "audio", "no");
  mpv_set_option_string(vid->mpv, "video-sync", "display-resample");
  // Pause by default - we'll control playback manually
  mpv_set_option_string(vid->mpv, "pause", "yes");

  if (mpv_initialize(vid->mpv) < 0) {
    goto error;
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
    goto error;
  }

  // Load file
  const char *cmd[] = {"loadfile", path, NULL};
  mpv_command(vid->mpv, cmd);

  // Create texture and framebuffer for rendering
  glGenTextures(1, &vid->tex_id);
  glBindTexture(GL_TEXTURE_2D, vid->tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Create framebuffer for rendering video to texture
  glGenFramebuffers(1, &vid->fbo);

  // Get wakeup FD for event loop
  vid->wakeup_fd = mpv_get_wakeup_pipe(vid->mpv);

  // Initialize timing
  vid->last_seek_time = -1.0;
  vid->duration = 0.0;

  return vid;

error:
  if (vid->mpv)
    mpv_destroy(vid->mpv);
  free(vid);
  return NULL;
}

void shader_video_update(shader_video *vid, double current_time) {
  if (!vid || !vid->mpv)
    return;

  // Get video duration if we don't have it yet
  if (vid->duration == 0.0) {
    double duration;
    if (mpv_get_property(vid->mpv, "duration", MPV_FORMAT_DOUBLE, &duration) >=
        0) {
      vid->duration = duration;
    }
  }

  // If we have duration, loop the time
  double video_time = current_time;
  if (vid->duration > 0.0) {
    video_time = fmod(current_time, vid->duration);
  }

  // Seek to the desired time if it's different from last frame
  // Add a small tolerance to avoid constant seeking
  if (fabs(video_time - vid->last_seek_time) > 0.01) {
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%.3f", video_time);
    const char *seek_cmd[] = {"seek", time_str, "absolute", NULL};
    mpv_command_async(vid->mpv, 0, seek_cmd);
    vid->last_seek_time = video_time;
  }

  // Process any pending mpv events
  mpv_event *event;
  while ((event = mpv_wait_event(vid->mpv, 0)) != NULL) {
    if (event->event_id == MPV_EVENT_NONE)
      break;

    // Handle video size changes
    if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
      int64_t width, height;
      if (mpv_get_property(vid->mpv, "video-params/w", MPV_FORMAT_INT64,
                           &width) >= 0 &&
          mpv_get_property(vid->mpv, "video-params/h", MPV_FORMAT_INT64,
                           &height) >= 0) {
        vid->width = (int)width;
        vid->height = (int)height;

        // Update texture size
        glBindTexture(GL_TEXTURE_2D, vid->tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid->width, vid->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      }
    }
  }
}

void shader_video_render(shader_video *vid) {
  if (!vid->mpv_gl || vid->width == 0 || vid->height == 0)
    return;

  // Save current framebuffer
  GLint current_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);

  // Bind our framebuffer and attach the texture
  glBindFramebuffer(GL_FRAMEBUFFER, vid->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         vid->tex_id, 0);

  // Set viewport
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glViewport(0, 0, vid->width, vid->height);

  // Render mpv frame to our texture
  mpv_opengl_fbo mpv_fbo = {
      .fbo = vid->fbo, .w = vid->width, .h = vid->height, .internal_format = 0};

  int flip = 1;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip},
      {MPV_RENDER_PARAM_INVALID, NULL},
  };

  mpv_render_context_render(vid->mpv_gl, params);

  // Restore previous framebuffer and viewport
  glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void shader_video_destroy(shader_video *vid) {
  if (!vid)
    return;

  free(vid->path);
  vid->path = NULL;

  if (vid->mpv_gl) {
    mpv_render_context_free(vid->mpv_gl);
  }

  if (vid->mpv) {
    mpv_destroy(vid->mpv);
  }

  if (vid->tex_id) {
    glDeleteTextures(1, &vid->tex_id);
  }

  if (vid->fbo) {
    glDeleteFramebuffers(1, &vid->fbo);
  }

  free(vid);
}
