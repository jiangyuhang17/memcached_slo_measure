#ifndef CONNECTIONOPTIONS_H
#define CONNECTIONOPTIONS_H

typedef struct {
  int time;
  int keysize;
  int valuesize;
  int records;
  double ratio;
  int connections;
  int depth;
} options_t;

#endif