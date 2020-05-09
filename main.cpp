#include <stdio.h>
#include <string.h>

#include <vector>

#include <event2/event.h>
#include <event2/dns.h>

#include "util.h"
#include "Connection.h"
#include "config.h"

char random_char[2 * 1024 * 1024];

void init_random_char() {
  char init_char[] = "The libevent API provides a mechanism to execute a callback function when a specific event occurs on a file descriptor or after a timeout has been reached. Furthermore, libevent also support callbacks due to signals or regular timeouts. libevent is meant to replace the event loop found in event driven network servers. An application just needs to call event_dispatch() and then add or remove events dynamically without having to change the event loop.";
  size_t cursor = 0;

  while (cursor < sizeof(random_char)) {
    size_t max = sizeof(init_char);
    if (sizeof(random_char) - cursor < max)
      max = sizeof(random_char) - cursor;
    memcpy(&random_char[cursor], init_char, max);
    cursor += max;
  }
}

void wait_until_idle(struct event_base* base, vector<Connection*> & connections) {
  while (1) {
    event_base_loop(base, EVLOOP_ONCE);

    bool restart = false;
    for (Connection *conn: connections)
      if (!conn->is_ready()) restart = true;

    if (restart) continue;
    else break;
  }
}

void run(options_t options, ConnectionStats stats) {
  struct event_base *base;
  struct evdns_base *evdns;

  DIE_Z(base = event_base_new());
  DIE_Z(evdns = evdns_base_new(base, 1));
  
  double start = get_time();
  double now = start;

  vector<Connection*> connections;
  vector<Connection*> server_lead;

  Connection *conn = new Connection(base, evdns, "127.0.0.1", 11211, options);
  connections.push_back(conn);
  server_lead.push_back(conn);

  wait_until_idle(base, connections);
  for (auto c: server_lead) c->start_loading();
  wait_until_idle(base, connections);

  start = get_time();
  for (Connection *conn: connections) {
    conn->start_time = start;
    conn->start();
  }

  while (1) {
    event_base_loop(base, EVLOOP_NONBLOCK);
    struct timeval now_tv;
    event_base_gettimeofday_cached(base, &now_tv);
    now = tv_to_double(&now_tv);

    bool restart = false;
    for (Connection *conn: connections)
      if (!conn->check_exit_condition(now))
        restart = true;

    if (restart) continue;
    else break;
  }

  for (Connection *conn: connections) {
    stats.accumulate(conn->stats);
    delete conn;
  }

  stats.start = start;
  stats.stop = now;

  evdns_base_free(evdns, 0);
  event_base_free(base);
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  init_random_char();

  options_t options;
  ConnectionStats stats;

  double peak_qps = 0.0;

  run(options, stats);

  stats.print_header();
  stats.print_stats("read",   stats.get_sampler);
  stats.print_stats("update", stats.set_sampler);
  stats.print_stats("op_q",   stats.op_sampler);

  int total = stats.gets + stats.sets;

  printf("\nTotal QPS = %.1f (%d / %.1fs)\n",
          total / (stats.stop - stats.start),
          total, stats.stop - stats.start);

  printf("\n");

  printf("Misses = %" PRIu64 " (%.1f%%)\n", stats.get_misses,
          (double) stats.get_misses/stats.gets*100);

  printf("Skipped TXs = %" PRIu64 " (%.1f%%)\n\n", stats.skips,
          (double) stats.skips / total * 100);

  printf("RX %10" PRIu64 " bytes : %6.1f MB/s\n",
          stats.rx_bytes,
          (double) stats.rx_bytes / 1024 / 1024 / (stats.stop - stats.start));
  printf("TX %10" PRIu64 " bytes : %6.1f MB/s\n",
          stats.tx_bytes,
          (double) stats.tx_bytes / 1024 / 1024 / (stats.stop - stats.start));

}