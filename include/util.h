#ifndef H_UTIL
#define H_UTIL

#include <time.h>

char *load_file(const char *path);

double timespec_to_sec(struct timespec ts);

struct timespec current_time();

double current_time_in_sec();

double time_elapsed(struct timespec start_time);

#endif
