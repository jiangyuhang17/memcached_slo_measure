#ifndef CONNECTIONSTATS_H
#define CONNECTIONSTATS_H

#include <inttypes.h>

#include "LogHistogramSampler.h"

using namespace std;

class ConnectionStats {
public:
  ConnectionStats() : get_sampler(200), set_sampler(200), op_sampler(100),
    rx_bytes(0), tx_bytes(0), gets(0), sets(0), get_misses(0), skips(0) {}
  
  LogHistogramSampler get_sampler;
  LogHistogramSampler set_sampler;
  LogHistogramSampler op_sampler;
  
  uint64_t rx_bytes, tx_bytes;  
  uint64_t gets, sets, get_misses;
  uint64_t skips;

  double start, stop;

  void log_get(Operation& op) { get_sampler.sample(op); gets++; }
  void log_set(Operation& op) { set_sampler.sample(op); sets++; }
  void log_op (double op)     { op_sampler.sample(op); }

  double get_qps() {
    return (gets + sets) / (stop - start);
  }

  double get_nth(double nth) {
    return get_sampler.get_nth(nth);
  }

  void accumulate(const ConnectionStats &cs) {
    get_sampler.accumulate(cs.get_sampler);
    set_sampler.accumulate(cs.set_sampler);
    op_sampler.accumulate(cs.op_sampler);

    rx_bytes += cs.rx_bytes;
    tx_bytes += cs.tx_bytes;
    gets += cs.gets;
    sets += cs.sets;
    get_misses += cs.get_misses;
    skips += cs.skips;

    start = cs.start;
    stop = cs.stop;
  }

  void print_header() {
    printf("%-7s %7s %7s %7s %7s %7s %7s %7s %7s\n",
           "#type", "avg", "std", "min", /*"1st",*/ "5th", "10th",
           "90th", "95th", "99th");
  }

  void print_stats(const char *tag, LogHistogramSampler &sampler,
                   bool newline = true) {
    if (sampler.total() == 0) {
      printf("%-7s %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f",
             tag, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      if (newline) printf("\n");
      return;
    }

    printf("%-7s %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f",
           tag, sampler.average(), sampler.stddev(),
           sampler.get_nth(0), /*sampler.get_nth(1),*/ sampler.get_nth(5),
           sampler.get_nth(10), sampler.get_nth(90),
           sampler.get_nth(95), sampler.get_nth(99));

    if (newline) printf("\n");
  }
};

#endif