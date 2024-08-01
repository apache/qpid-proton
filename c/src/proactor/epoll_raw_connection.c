/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

/* This is currently epoll implementation specific - and will need changing for the other proactors */

#include "epoll-internal.h"
#include "proactor-internal.h"
#include "raw_connection-internal.h"

#include <proton/proactor.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/raw_connection.h>

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>

/* epoll specific raw connection struct */
struct praw_connection_t {
  task_t task;
  struct pn_raw_connection_t raw_connection;
  psocket_t psocket;
  struct pn_netaddr_t local, remote; /* Actual addresses */
  pmutex rearm_mutex;                /* protects pconnection_rearm from out of order arming*/
  pn_event_batch_t batch;
  struct addrinfo *addrinfo;         /* Resolved address list */
  struct addrinfo *ai;               /* Current connect address */
  int current_arm;                   /* Active epoll io events */
  bool armed;
  bool connected;
  bool disconnected;
  bool hup_detected;
  bool read_check;
  bool first_schedule;
  char *taddr;
};

static void psocket_error(praw_connection_t *rc, int err, const char* msg) {
  pn_condition_t *cond = rc->raw_connection.condition;
  if (!pn_condition_is_set(cond)) { /* Preserve older error information */
    strerrorbuf what;
    pstrerror(err, what);
    char addr[PN_MAX_ADDR];
    pn_netaddr_str(&rc->remote, addr, sizeof(addr));
    pn_condition_format(cond, PNI_IO_CONDITION, "%s - %s %s", what, msg, addr);
  }
}

static void psocket_gai_error(praw_connection_t *rc, int gai_err, const char* what, const char *addr) {
  pn_condition_format(rc->raw_connection.condition, PNI_IO_CONDITION, "%s - %s %s",
                      gai_strerror(gai_err), what, addr);
}

static void praw_connection_connected_lh(praw_connection_t *prc) {
  // Need to check socket for connection error
  prc->connected = true;
  if (prc->addrinfo) {
    freeaddrinfo(prc->addrinfo);
    prc->addrinfo = NULL;
  }
  prc->ai = NULL;
  socklen_t len = sizeof(prc->remote.ss);
  (void)getpeername(prc->psocket.epoll_io.fd, (struct sockaddr*)&prc->remote.ss, &len);

  pni_raw_connected(&prc->raw_connection);
}

/* multi-address connections may call pconnection_start multiple times with diffferent FDs  */
static void praw_connection_start(praw_connection_t *prc, int fd) {
  int efd = prc->task.proactor->epollfd;

  /* Get the local socket name now, get the peer name in pconnection_connected */
  socklen_t len = sizeof(prc->local.ss);
  (void)getsockname(fd, (struct sockaddr*)&prc->local.ss, &len);

  epoll_extended_t *ee = &prc->psocket.epoll_io;
  if (ee->polling) {     /* This is not the first attempt, stop polling and close the old FD */
    int fd = ee->fd;     /* Save fd, it will be set to -1 by stop_polling */
    stop_polling(ee, efd);
    pclosefd(prc->task.proactor, fd);
  }
  ee->fd = fd;
  prc->current_arm = ee->wanted = EPOLLIN | EPOLLOUT;
  prc->armed = true;
  start_polling(ee, efd);  // TODO: check for error
}

/* Called on initial connect, and if connection fails to try another address */
static void praw_connection_maybe_connect_lh(praw_connection_t *prc) {
  while (prc->ai) {            /* Have an address */
    struct addrinfo *ai = prc->ai;
    prc->ai = prc->ai->ai_next; /* Move to next address in case this fails */
    int fd = socket(ai->ai_family, SOCK_STREAM, 0);
    if (fd >= 0) {
      configure_socket(fd);
      if (!connect(fd, ai->ai_addr, ai->ai_addrlen) || errno == EINPROGRESS) {

        /* Until we finish connecting save away the address we're trying to connect to */
        memcpy((struct sockaddr *) &prc->remote.ss, ai->ai_addr, ai->ai_addrlen);

        praw_connection_start(prc, fd);
        return;               /* Async connection started */
      } else {
        close(fd);
      }
    }
    /* connect failed immediately, go round the loop to try the next addr */
  }
  int err;
  socklen_t errlen = sizeof(err);
  getsockopt(prc->psocket.epoll_io.fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);
  psocket_error(prc, err, "on connect");

  freeaddrinfo(prc->addrinfo);
  prc->addrinfo = NULL;
  prc->disconnected = true;
}

//
// Raw socket API
//
static pn_event_t * pni_raw_batch_next(pn_event_batch_t *batch);

static void praw_connection_init(praw_connection_t *prc, pn_proactor_t *p, pn_raw_connection_t *rc) {
  task_init(&prc->task, RAW_CONNECTION, p);
  psocket_init(&prc->psocket, RAW_CONNECTION_IO);

  prc->connected = false;
  prc->disconnected = false;
  prc->first_schedule = false;
  prc->armed = false;
  prc->current_arm = 0;
  prc->taddr = NULL;
  prc->batch.next_event = pni_raw_batch_next;

  pmutex_init(&prc->rearm_mutex);
}

static void praw_connection_cleanup(praw_connection_t *prc) {
  int fd = prc->psocket.epoll_io.fd;
  stop_polling(&prc->psocket.epoll_io, prc->task.proactor->epollfd);
  if (fd != -1)
    pclosefd(prc->task.proactor, fd);

  lock(&prc->task.mutex);
  bool can_free = proactor_remove(&prc->task);
  unlock(&prc->task.mutex);
  if (can_free) {
    task_finalize(&prc->task);
    if (prc->addrinfo)
      freeaddrinfo(prc->addrinfo);
    free(prc->taddr);
    free(prc);
  }
  // else proactor_disconnect logic owns prc and its final free
}

static void praw_initiate_cleanup(praw_connection_t *prc) {
  if (prc->armed) {
    // Possible race with epoll event.  Wait for it to clear.
    // Force EPOLLHUP callback if not already pending.
    shutdown(prc->psocket.epoll_io.fd, SHUT_RDWR);
    return;
  }
  pni_raw_finalize(&prc->raw_connection);
  praw_connection_cleanup(prc);
}

pn_raw_connection_t *pn_raw_connection(void) {
  praw_connection_t *conn = (praw_connection_t*) calloc(1, sizeof(praw_connection_t));
  if (!conn) return NULL;

  pni_raw_initialize(&conn->raw_connection);

  return &conn->raw_connection;
}

// Call from pconnection_process with task lock held.
// Return true if the socket is connecting and there are no Proton events to deliver.
static bool praw_connection_first_connect_lh(praw_connection_t *prc) {
  const char *host;
  const char *port;

  unlock(&prc->task.mutex);
  size_t addrlen = strlen(prc->taddr);
  char *addr_buf = (char*) alloca(addrlen+1);
  pni_parse_addr(prc->taddr, addr_buf, addrlen+1, &host, &port);
  // TODO: move this step to a separate worker thread that scales in response to multiple blocking DNS lookups.
  int gai_error = pgetaddrinfo(host, port, 0, &prc->addrinfo);
  lock(&prc->task.mutex);

  if (!gai_error) {
    prc->ai = prc->addrinfo;
    praw_connection_maybe_connect_lh(prc); /* Start connection attempts */
    if (prc->psocket.epoll_io.fd != -1 && !pni_task_wake_pending(&prc->task))
      return true;
  } else {
    psocket_gai_error(prc, gai_error, "connect to ", prc->taddr);
  }
  return false;
}

void pn_proactor_raw_connect(pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr) {
  // Called from an arbitrary thread.  Do setup prior to getaddrinfo, then switch to a worker thread.
  assert(rc);
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  praw_connection_init(prc, p, rc);
  // TODO: check case of proactor shutting down

  lock(&prc->task.mutex);
  size_t addrlen = strlen(addr);
  prc->taddr = (char*) malloc(addrlen+1);
  assert(prc->taddr); // TODO: memory safety
  memcpy(prc->taddr, addr, addrlen+1);
  prc->first_schedule = true; // Resume connection setup when next scheduled.
  proactor_add(&prc->task);
  bool notify = schedule(&prc->task);
  unlock(&prc->task.mutex);

  if (notify) notify_poller(p);
}

void pn_listener_raw_accept(pn_listener_t *l, pn_raw_connection_t *rc) {
  assert(rc);
  pn_proactor_t *p = pn_listener_proactor(l);
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  praw_connection_init(prc, p, rc);
  // TODO: fuller sanity check on input args

  int err = 0;
  int fd = -1;
  bool notify = false;
  lock(&l->task.mutex);
  if (l->task.closing)
    err = EBADF;
  else {
    accepted_t *a = listener_accepted_next(l);
    if (a) {
      fd = a->accepted_fd;
      a->accepted_fd = -1;
    }
    else err = EWOULDBLOCK;
  }

  proactor_add(&prc->task);

  lock(&prc->task.mutex);
  if (fd >= 0) {
    configure_socket(fd);
    praw_connection_start(prc, fd);
    praw_connection_connected_lh(prc);
  } else {
    psocket_error(prc, err, "pn_listener_accept");
    pni_raw_connect_failed(&prc->raw_connection);
    notify = schedule(&prc->task);
  }

  if (!l->task.working && listener_has_event(l)) {
    notify |= schedule(&l->task);
  }
  unlock(&prc->task.mutex);
  unlock(&l->task.mutex);
  if (notify) notify_poller(p);
}

const pn_netaddr_t *pn_raw_connection_local_addr(pn_raw_connection_t *rc) {
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  if (!prc) return NULL;
  return &prc->local;
}

const pn_netaddr_t *pn_raw_connection_remote_addr(pn_raw_connection_t *rc) {
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  if (!prc) return NULL;
  return &prc->remote;
}

void pn_raw_connection_wake(pn_raw_connection_t *rc) {
  bool notify = false;
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  pn_proactor_t *p = prc->task.proactor;
  lock(&prc->task.mutex);
  if (!prc->task.closing) {
    notify = pni_task_wake(&prc->task);
  }
  unlock(&prc->task.mutex);
  if (notify) notify_poller(p);
}

static inline void set_closed(pn_raw_connection_t *rc)
{
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  lock(&prc->task.mutex);
  prc->task.closing = true;
  unlock(&prc->task.mutex);
}

void pn_raw_connection_close(pn_raw_connection_t *rc) {
  set_closed(rc);
  pni_raw_close(rc);
}

void pn_raw_connection_read_close(pn_raw_connection_t *rc) {
  if (pn_raw_connection_is_write_closed(rc)) {
    set_closed(rc);
  }
  pni_raw_read_close(rc);
}

void pn_raw_connection_write_close(pn_raw_connection_t *rc) {
  if (pn_raw_connection_is_read_closed(rc)) {
    set_closed(rc);
  }
  pni_raw_write_close(rc);
}

static pn_event_t *pni_raw_batch_next(pn_event_batch_t *batch) {
  praw_connection_t *rc = containerof(batch, praw_connection_t, batch);
  pn_raw_connection_t *raw = &rc->raw_connection;

  // Check wake status every event processed
  bool waking = false;
  lock(&rc->task.mutex);
  if (pni_task_wake_pending(&rc->task)) {
    waking = true;
    pni_task_wake_done(&rc->task);
  }
  unlock(&rc->task.mutex);
  if (waking) pni_raw_wake(raw);

  return pni_raw_event_next(raw);
}

task_t *pni_psocket_raw_task(psocket_t* ps) {
  return &containerof(ps, praw_connection_t, psocket)->task;
}

praw_connection_t *pni_task_raw_connection(task_t *t) {
  return containerof(t, praw_connection_t, task);
}

psocket_t *pni_task_raw_psocket(task_t *t) {
  return &containerof(t, praw_connection_t, task)->psocket;
}

praw_connection_t *pni_batch_raw_connection(pn_event_batch_t *batch) {
  return (batch->next_event == pni_raw_batch_next) ?
    containerof(batch, praw_connection_t, batch) : NULL;
}

pn_raw_connection_t *pn_event_batch_raw_connection(pn_event_batch_t *batch) {
    praw_connection_t *rc = pni_batch_raw_connection(batch);
    return rc ? &rc->raw_connection : NULL;
}

task_t *pni_raw_connection_task(praw_connection_t *rc) {
  return &rc->task;
}

static long snd(int fd, const void* b, size_t s, bool more) {
  int flags = MSG_NOSIGNAL | MSG_DONTWAIT;
  if (more) flags |= MSG_MORE;
  return send(fd, b, s, flags);
}

static long rcv(int fd, void* b, size_t s) {
  return recv(fd, b, s, MSG_DONTWAIT);
}

static int shutr(int fd) {
  return shutdown(fd, SHUT_RD);
}

static int shutw(int fd) {
  return shutdown(fd, SHUT_WR);
}

static void  set_error(pn_raw_connection_t *conn, const char *msg, int err) {
  psocket_error(containerof(conn, praw_connection_t, raw_connection), err, msg);
}

pn_event_batch_t *pni_raw_connection_process(task_t *t, uint32_t io_events, bool sched_ready) {
  praw_connection_t *rc = containerof(t, praw_connection_t, task);
  bool task_wake = false;
  bool can_wake = pni_raw_can_wake(&rc->raw_connection);
  lock(&rc->task.mutex);
  t->working = true;
  if (sched_ready)
    schedule_done(t);
  if (pni_task_wake_pending(&rc->task)) {
    if (can_wake)
      task_wake = true; // batch_next() will complete the task wake.
    else
      pni_task_wake_done(&rc->task);  // Complete task wake without event.
  }
  if (io_events) {
    rc->armed = false;
    rc->current_arm = 0;
  }
  if (pni_raw_finished(&rc->raw_connection)) {
    unlock(&rc->task.mutex);
    praw_initiate_cleanup(rc);
    return NULL;
  }
  int events = io_events;
  int fd = rc->psocket.epoll_io.fd;

  if (rc->first_schedule) {
    rc->first_schedule = false;
    assert(!events); // No socket yet.
    assert(!rc->connected);
    if (praw_connection_first_connect_lh(rc)) {
      unlock(&rc->task.mutex);
      return NULL;
    }
  }
  if (!rc->connected) {
    if (events & (EPOLLHUP | EPOLLERR)) {
      praw_connection_maybe_connect_lh(rc);
    }
    if (rc->disconnected) {
      pni_raw_connect_failed(&rc->raw_connection);
      unlock(&rc->task.mutex);
      return &rc->batch;
    }
    if (events & (EPOLLHUP | EPOLLERR)) {
      // Continuation of praw_connection_maybe_connect_lh() logic.
      // A wake can be the first event.  Otherwise, wait for connection to complete.
      bool event_pending = task_wake || pni_raw_wake_is_pending(&rc->raw_connection) || pn_collector_peek(rc->raw_connection.collector);
      t->working = event_pending;
      unlock(&rc->task.mutex);
      return event_pending ? &rc->batch : NULL;
    }
    if (events & EPOLLOUT)
      praw_connection_connected_lh(rc);

    unlock(&rc->task.mutex);
    return &rc->batch;
  }
  unlock(&rc->task.mutex);

  if (events & EPOLLERR) {
    // Read and write sides closed via RST.  Tear down immediately.
    int soerr;
    socklen_t soerrlen = sizeof(soerr);
    int ec = getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &soerrlen);
    if (ec == 0 && soerr) {
      psocket_error(rc, soerr, "async disconnect");
    }
    pni_raw_async_disconnect(&rc->raw_connection);
    return &rc->batch;
  }
  if (events & EPOLLHUP) {
    rc->hup_detected = true;
  }

  if (events & (EPOLLIN | EPOLLRDHUP) || rc->read_check) {
    pni_raw_read(&rc->raw_connection, fd, rcv, set_error);
    rc->read_check = false;
  }
  if (events & EPOLLOUT) pni_raw_write(&rc->raw_connection, fd, snd, set_error);
  return &rc->batch;
}

void pni_raw_connection_done(praw_connection_t *rc) {
  bool notify = false;
  bool ready = false;
  pn_raw_connection_t *raw = &rc->raw_connection;
  int fd = rc->psocket.epoll_io.fd;

  // Try write
  if (pni_raw_can_write(raw)) pni_raw_write(raw, fd, snd, set_error);
  pni_raw_process_shutdown(raw, fd, shutr, shutw);

  // Update state machine and check for possible pending event.
  bool have_event = pni_raw_batch_has_events(raw);

  lock(&rc->task.mutex);
  bool wake_pending = pni_task_wake_pending(&rc->task) && pni_raw_can_wake(raw);
  pn_proactor_t *p = rc->task.proactor;
  tslot_t *ts = rc->task.runner;
  rc->task.working = false;
  // The task may be in the ready state even if we've got no raw connection
  // wakes outstanding because we dealt with it already in pni_raw_batch_next()
  notify = (wake_pending || have_event) && schedule(&rc->task);
  ready = rc->task.ready;  // No need to poll.  Already scheduled.
  unlock(&rc->task.mutex);

  if (pni_raw_finished(raw) && !ready) {
    // If raw connection has no more work to do and safe to free resources, do so.
    praw_initiate_cleanup(rc);
  } else if (ready) {
    // Already scheduled to run.  Skip poll.  Remember if we want a read.
    rc->read_check = pni_raw_can_read(raw);
  } else if (!rc->connected) {
    // Connect logic has already armed the socket.
  } else {
    // Must poll for IO.
    int wanted =
      (pni_raw_can_read(raw)  ? (EPOLLIN | EPOLLRDHUP) : 0) |
      (pni_raw_can_write(raw) ? EPOLLOUT : 0);

    // wanted == 0 implies we block until either application wake() or EPOLLHUP | EPOLLERR.
    // If wanted == 0 and hup_detected, blocking not possible, so skip arming until
    // application provides read buffers.
    if (wanted || !rc->hup_detected) {
      // Arm only if there is a change.
      if (!rc->armed || (wanted != rc->current_arm)) {
        rc->psocket.epoll_io.wanted = rc->current_arm = wanted;
        rc->armed = true;
        rearm_polling(&rc->psocket.epoll_io, p->epollfd);  // TODO: check for error
      }
    }
  }

  // praw_initiate_cleanup() may have been called above. Can no longer touch rc or raw.

  lock(&p->sched_mutex);
  tslot_t *resume_thread;
  notify |= unassign_thread(p, ts, UNUSED, &resume_thread);
  unlock(&p->sched_mutex);
  if (notify) notify_poller(p);
  if (resume_thread) pni_resume(p, resume_thread);
}

void pni_raw_connection_forced_shutdown(praw_connection_t *rc) {
  rc->armed = false;  // Tear down. No epoll event callbacks.
  praw_initiate_cleanup(rc);
}
