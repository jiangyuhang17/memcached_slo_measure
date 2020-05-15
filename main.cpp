#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <iostream>

#include <event2/event.h>
#include <event2/dns.h>

#include "util.h"
#include "Connection.h"
#include "config.h"
#include "cmdline.h"

char random_char[2 * 1024 * 1024];
gengetopt_args_info args;

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

void args_to_options(options_t* options) {
  options->time = args.time_arg;
  options->keysize = args.keysize_arg;
  options->valuesize = args.valuesize_arg;
  options->records = args.records_arg / args.server_given;
  if (!options->records) options->records = 1;
  options->ratio = args.ratio_arg;
  options->connections = args.connections_arg;
  options->depth = args.depth_arg;
}

pair<string, int> string_to_addr(string host) {
  char *s_copy = new char[host.length() + 1];
  strcpy(s_copy, host.c_str());

  char *save_ptr = NULL;
  char *h_ptr = strtok_r(s_copy, ":", &save_ptr);
  char *p_ptr = strtok_r(NULL, ":", &save_ptr);

  char ipaddr[16];
  char buf[100];
  if (h_ptr == NULL) {
    snprintf(buf, 100, "strtok(.., \":\") failed to parse %s", host.c_str());
    die(buf);
  }

  int port = 11211;
  if (p_ptr) 
    port = atoi(p_ptr);

  struct evutil_addrinfo hints;
  struct evutil_addrinfo *addr = NULL;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = EVUTIL_AI_ADDRCONFIG;

  DIE_NE(evutil_getaddrinfo(h_ptr, NULL, &hints, &addr));
  if (addr == NULL) 
    die("No DNS answer.");

  void *ptr = &((struct sockaddr_in *) addr->ai_addr)->sin_addr;;

  inet_ntop(addr->ai_family, ptr, ipaddr, 16);

  pair<string, int> server {string(ipaddr), port};

  return server;
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

void run(const vector<pair<string, int>>& servers, options_t& options, ConnectionStats& stats) {
  struct event_base *base;
  struct evdns_base *evdns;

  DIE_Z(base = event_base_new());
  DIE_Z(evdns = evdns_base_new(base, 1));
  
  double start = get_time();
  double now = start;

  vector<Connection*> connections;
  vector<Connection*> server_lead;

  for (auto s: servers) {
    for (int c = 0; c < options.connections; c++) {
      Connection *conn = new Connection(base, evdns, s.first, s.second, options);
      connections.push_back(conn);
      if (c == 0) server_lead.push_back(conn);
    }   
  }

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
  DIE_NZ(cmdline_parser(argc, argv, &args));

  char buf[100];
  if (args.depth_arg < 1) 
    die("--depth must be >= 1");
  if (args.ratio_arg < 0.0 || args.ratio_arg > 1.0) 
    die("--update must be >= 0.0 and <= 1.0");
  if (args.time_arg < 1) 
    die("--time must be >= 1");
  if (args.keysize_arg < MINIMUM_KEY_LENGTH) {
    snprintf(buf, 100, "--keysize must be >= %d", MINIMUM_KEY_LENGTH);
    die(buf);
  }
  if (args.connections_arg < 1 || args.connections_arg > MAXIMUM_CONNECTIONS) {
    snprintf(buf, 100, "--connections must be between [1,%d]", MAXIMUM_CONNECTIONS);
    die(buf);
  }
  if (args.server_given == 0)
    die("--server must be specified.");

  setvbuf(stdout, NULL, _IONBF, 0);
  init_random_char();

  options_t options;
  args_to_options(&options);  

  vector<pair<string, int>> servers;
  for (unsigned int s = 0; s < args.server_given; s++)
    servers.push_back(string_to_addr(string(args.server_arg[s])));

  ConnectionStats stats;

  run(servers, options, stats);

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

  cmdline_parser_free(&args);
}