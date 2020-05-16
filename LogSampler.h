/* -*- c++ -*- */
#ifndef LOGSAMPLER_H
#define LOGSAMPLER_H

#include <assert.h>
#include <inttypes.h>
#include <math.h>

#include <vector>

#include "Operation.h"
#include "util.h"

#define _POW 1.1

class LogSampler {
public:
  std::vector<uint64_t> bins;

  std::vector<Operation> samples;

  double sum;
  double sum_sq;

  LogSampler() = delete;
  LogSampler(int _bins) : sum(0.0), sum_sq(0.0) {
    assert(_bins > 0);

    bins.resize(_bins + 1, 0);
  }

  void sample(const Operation &op) {
    sample(op.time());
  }

  void sample(double s) {
    assert(s >= 0);
    size_t bin = log(s)/log(_POW);

    sum += s;
    sum_sq += s*s;

    if ((int64_t) bin < 0) {
      bin = 0;
    } else if (bin >= bins.size()) {
      bin = bins.size() - 1;
    }

    bins[bin]++;
  }

  double average() {
    return sum / total();
  }

  double stddev() {
    return sqrt(sum_sq / total() - pow(sum / total(), 2.0));
  }

  double minimum() {
    for (size_t i = 0; i < bins.size(); i++)
      if (bins[i] > 0) return pow(_POW, (double) i + 0.5);
    die("Not implemented");
    return 0.0;
  }

  double get_nth(double nth) {
    uint64_t count = total();
    uint64_t n = 0;
    double target = count * nth/100;

    for (size_t i = 0; i < bins.size(); i++) {
      n += bins[i];

      if (n > target) {
        double left = target - (n - bins[i]);
        return pow(_POW, (double) i) +
          left / bins[i] * (pow(_POW, (double) (i+1)) - pow(_POW, (double) i));
      }
    }

    return pow(_POW, bins.size());
  } 

  uint64_t total() {
    uint64_t sum = 0.0;

    for (auto i: bins) sum += i;

    return sum;
  }

  void accumulate(const LogSampler &h) {
    assert(bins.size() == h.bins.size());

    for (size_t i = 0; i < bins.size(); i++) bins[i] += h.bins[i];

    sum += h.sum;
    sum_sq += h.sum_sq;

    for (auto i: h.samples) samples.push_back(i);
  }
};

#endif