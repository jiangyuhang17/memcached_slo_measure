#ifndef CONNECTIONOPTIONS_H
#define CONNECTIONOPTIONS_H

typedef struct {
  int records = 10000;
  double update = 0.5;
  int time = 5;
  int depth = 1;
} options_t;

#endif