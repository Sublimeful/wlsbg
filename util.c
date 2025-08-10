#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char *load_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  fread(buffer, 1, length, file);
  buffer[length] = '\0';
  fclose(file);
  return buffer;
}

double timespec_to_sec(struct timespec ts) {
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

struct timespec current_time() {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now;
}

double current_time_in_sec() { return timespec_to_sec(current_time()); }

double time_elapsed(struct timespec start_time) {
  return current_time_in_sec() - timespec_to_sec(start_time);
}
