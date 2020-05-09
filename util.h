#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define DIE_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define DIE_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)
#define DIE_NE(x) do { if ((x) < 0) die("error: " #x " failed (returned negative)." ); } while (0)

void die(const char *reason);

inline double tv_to_double(struct timeval *tv) {
  return tv->tv_sec + (double) tv->tv_usec / 1000000;
}

inline double get_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv_to_double(&tv);
}

#endif