#include <netinet/tcp.h>
#include <string.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <evutil.h>

#include "Connection.h"
#include "util.h"
#include "config.h"

Connection::Connection(struct event_base* _base, struct evdns_base* _evdns, 
                      string _hostname, int _port, options_t _options) :
  base(_base), evdns(_evdns), hostname(_hostname), port(_port), options(_options)
{
  read_state  = INIT_READ;
  write_state = INIT_WRITE;

  bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, bev_read_cb, bev_write_cb, bev_event_cb, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  prot = new ProtocolMemcachedText(this, bev);

  DIE_NZ(bufferevent_socket_connect_hostname(bev, evdns, AF_INET, hostname.c_str(), port));
}

Connection::~Connection() {
  bufferevent_free(bev);
}

void Connection::reset() {
  assert(op_queue.size() == 0);
  read_state = IDLE;
  write_state = INIT_WRITE;
  stats = ConnectionStats();
}

void Connection::start_loading() {
  read_state = LOADING;
  loader_issued = loader_completed = 0;

  for (int i = 0; i < LOADER_CHUNK; i++) {
    if (loader_issued >= options.records) break;
    char key[256];
    int index = lrand48() % (1024 * 1024);
    snprintf(key, 256, "%0*" PRIu64, 30, loader_issued);
    issue_set(key, &random_char[index], 200);
    loader_issued++;
  }
}

void Connection::issue_set_or_get(double now) {
  char key[256];
  snprintf(key, 256, "%0*" PRIu64, 30, loader_issued);

  if (drand48() < options.update) {
    int index = lrand48() % (1024 * 1024);
    issue_set(key, &random_char[index], 200, now);
  } else {
    issue_get(key, now);
  }
}

void Connection::issue_get(const char* key, double now) {
  Operation op;
  int l;

  if (now == 0.0) {
    op.start_time = get_time();
  } else {
    op.start_time = now;
  }

  op.key = string(key);
  op.type = Operation::GET;
  op_queue.push(op);

  if (read_state == IDLE) read_state = WAITING_FOR_GET;
  l = prot->get_request(key);
  if (read_state != LOADING) stats.tx_bytes += l;
}

void Connection::issue_set(const char* key, const char* value, int length,
                           double now) {
  Operation op;
  int l;

  if (now == 0.0) op.start_time = get_time();
  else op.start_time = now;

  op.type = Operation::SET;
  op_queue.push(op);

  if (read_state == IDLE) read_state = WAITING_FOR_SET;
  l = prot->set_request(key, value, length);
  if (read_state != LOADING) stats.tx_bytes += l;
}

void Connection::pop_op() {
  assert(op_queue.size() > 0);

  op_queue.pop();

  if (read_state == LOADING) return;
  read_state = IDLE;

  if (op_queue.size() > 0) {
    Operation& op = op_queue.front();
    switch (op.type) {
    case Operation::GET: read_state = WAITING_FOR_GET; break;
    case Operation::SET: read_state = WAITING_FOR_SET; break;
    default: die("Not implemented.");
    }
  }
}

void Connection::finish_op(Operation *op) {
  double now;
  now = get_time();
  op->end_time = now;

  switch (op->type) {
  case Operation::GET: stats.log_get(*op); break;
  case Operation::SET: stats.log_set(*op); break;
  default: die("Not implemented.");
  }

  pop_op();
  drive_write_machine();
}

void Connection::event_callback(short events) {
  if (events & BEV_EVENT_CONNECTED) {
    int fd;
    DIE_NE(fd = bufferevent_getfd(bev));
    int one = 1;
    DIE_NZ(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one)));
    read_state = IDLE;
  } else if (events & BEV_EVENT_ERROR) {
    int err = bufferevent_socket_get_dns_error(bev);
    char buf[100];
    if (err) {
      snprintf(buf, 100, "DNS error: %s", evutil_gai_strerror(err));
      die(buf);
    }
    snprintf(buf, 100, "BEV_EVENT_ERROR: %s", strerror(errno));
    die(buf);
  } else if (events & BEV_EVENT_EOF) {
    die("Unexpected EOF from server.");
  }
}

void Connection::read_callback() {
  struct evbuffer *input = bufferevent_get_input(bev);

  Operation *op = NULL;
  bool done, full_read;

  if (op_queue.size() == 0) die("Spurious read callback.");

  while (1) {
    if (op_queue.size() > 0) op = &op_queue.front();

    switch (read_state) {
    case INIT_READ: die("event from uninitialized connection");
    case IDLE: return;

    case WAITING_FOR_GET:
      assert(op_queue.size() > 0);
      full_read = prot->handle_response(input, done);
      if (!full_read) {
        return;
      } else if (done) {
        finish_op(op);
      }
      break;

    case WAITING_FOR_SET:
      assert(op_queue.size() > 0);
      if (!prot->handle_response(input, done)) return;
      finish_op(op);
      break;

    case LOADING:
      assert(op_queue.size() > 0);
      if (!prot->handle_response(input, done)) return;
      loader_completed++;
      pop_op();

      if (loader_completed == options.records) {
        read_state = IDLE;
      } else {
        while (loader_issued < loader_completed + LOADER_CHUNK) {
          if (loader_issued >= options.records) break;
          char key[256];
          int index = lrand48() % (1024 * 1024);
          snprintf(key, 256, "%0*" PRIu64, 30, loader_issued);
          issue_set(key, &random_char[index], 200);
          loader_issued++;
        }
      }

      break;
    default: die("not implemented");
    }
  }
}

void Connection::write_callback() {}

void Connection::drive_write_machine(double now) {
  if (now == 0.0) now = get_time();

  if (check_exit_condition(now)) return;

  while (1) {
    switch (write_state) {
    case INIT_WRITE:
      write_state = ISSUING;
      break;

    case ISSUING:
      if (op_queue.size() >= (size_t) options.depth) {
        write_state = WAITING_FOR_OPQ;
        return;
      }
      issue_set_or_get(now);
      stats.log_op(op_queue.size());
      break;

    case WAITING_FOR_OPQ:
      if (op_queue.size() >= (size_t) options.depth) return;
      write_state = ISSUING;
      break;

    default: die("Not implemented");
    }
  }
}

bool Connection::check_exit_condition(double now) {
  if (read_state == INIT_READ) return false;
  if (now == 0.0) now = get_time();
  if (now > start_time + options.time) return true;
  return false;
}

void bev_event_cb(struct bufferevent *bev, short events, void *ptr) {
  Connection* conn = (Connection*) ptr;
  conn->event_callback(events);
}

void bev_read_cb(struct bufferevent *bev, void *ptr) {
  Connection* conn = (Connection*) ptr;
  conn->read_callback();
}

void bev_write_cb(struct bufferevent *bev, void *ptr) {
  Connection* conn = (Connection*) ptr;
  conn->write_callback();
}