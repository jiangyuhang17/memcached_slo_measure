#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>
#include <queue>

#include <event2/event.h>
#include <event2/dns.h>
#include <event2/bufferevent.h>

#include "ConnectionOptions.h"
#include "ConnectionStats.h"
#include "Operation.h"
#include "Protocol.h"

using namespace std;

void bev_event_cb(struct bufferevent *bev, short events, void *ptr);
void bev_read_cb(struct bufferevent *bev, void *ptr);
void bev_write_cb(struct bufferevent *bev, void *ptr);

class Connection {
public:
  Connection(struct event_base* _base, struct evdns_base* _evdns, 
             string _hostname, int _port, options_t _options);
  ~Connection();

  double start_time;
  options_t options;
  ConnectionStats stats;

  bool is_ready() { return read_state == IDLE; }
  void start() { drive_write_machine(); }
  void start_loading();
  void reset();
  bool check_exit_condition(double now = 0.0);

  void event_callback(short events);
  void read_callback();
  void write_callback();

private:
  string hostname;
  int port;

  struct event_base *base;
  struct evdns_base *evdns;
  struct bufferevent *bev;

  enum read_state_enum {
    INIT_READ,
    LOADING,
    IDLE,
    WAITING_FOR_GET,
    WAITING_FOR_SET,
    MAX_READ_STATE,
  };

  enum write_state_enum {
    INIT_WRITE,
    ISSUING,
    WAITING_FOR_TIME,
    WAITING_FOR_OPQ,
    MAX_WRITE_STATE,
  };

  read_state_enum read_state;
  write_state_enum write_state;  

  int loader_issued, loader_completed;

  Protocol *prot;
  queue<Operation> op_queue;

  void pop_op();
  void finish_op(Operation *op);
  void drive_write_machine(double now = 0.0);

  void issue_get(const char* key, double now = 0.0);
  void issue_set(const char* key, const char* value, int length, double now = 0.0);
  void issue_set_or_get(double now = 0.0);
};

#endif