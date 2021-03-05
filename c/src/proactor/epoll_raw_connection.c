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
  bool connected;
  bool disconnected;
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
  ee->wanted = EPOLLIN | EPOLLOUT;
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
    free(prc);
  }
  // else proactor_disconnect logic owns prc and its final free
}

pn_raw_connection_t *pn_raw_connection(void) {
  praw_connection_t *conn = (praw_connection_t*) calloc(1, sizeof(praw_connection_t));
  if (!conn) return NULL;

  pni_raw_initialize(&conn->raw_connection);

  return &conn->raw_connection;
}

void pn_proactor_raw_connect(pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr) {
  assert(rc);
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  praw_connection_init(prc, p, rc);
  // TODO: check case of proactor shutting down

  lock(&prc->task.mutex);
  proactor_add(&prc->task);

  bool notify = false;

  const char *host;
  const char *port;
  size_t addrlen = strlen(addr);
  char *addr_buf = (char*) alloca(addrlen+1);
  pni_parse_addr(addr, addr_buf, addrlen+1, &host, &port);

  int gai_error = pgetaddrinfo(host, port, 0, &prc->addrinfo);
  if (!gai_error) {
    prc->ai = prc->addrinfo;
    praw_connection_maybe_connect_lh(prc); /* Start connection attempts */
    if (prc->disconnected) notify = schedule(&prc->task);
  } else {
    psocket_gai_error(prc, gai_error, "connect to ", addr);
    prc->disconnected = true;
    notify = schedule(&prc->task);
    lock(&p->task.mutex);
    notify |= schedule_if_inactive(p);
    unlock(&p->task.mutex);
  }

  /* We need to issue INACTIVE on immediate failure */
  unlock(&prc->task.mutex);
  if (notify) notify_poller(p);
}

void pn_listener_raw_accept(pn_listener_t *l, pn_raw_connection_t *rc) {
  assert(rc);
  praw_connection_t *prc = containerof(rc, praw_connection_t, raw_connection);
  praw_connection_init(prc, pn_listener_proactor(l), rc);
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
  }

  if (!l->task.working && listener_has_event(l)) {
    notify = schedule(&l->task);
  }
  unlock(&prc->task.mutex);
  unlock(&l->task.mutex);
  if (notify) notify_poller(l->task.proactor);
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
  lock(&prc->task.mutex);
  if (!prc->task.closing) {
    notify = pni_task_wake(&prc->task);
  }
  unlock(&prc->task.mutex);
  if (notify) notify_poller(prc->task.proactor);
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

praw_connection_t *pni_batch_raw_connection(pn_event_batch_t *batch) {
  return (batch->next_event == pni_raw_batch_next) ?
    containerof(batch, praw_connection_t, batch) : NULL;
}

task_t *pni_raw_connection_task(praw_connection_t *rc) {
  return &rc->task;
}

static long snd(int fd, const void* b, size_t s) {
  return send(fd, b, s, MSG_NOSIGNAL | MSG_DONTWAIT);
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

pn_event_batch_t *pni_raw_connection_process(task_t *t, bool sched_ready) {
  praw_connection_t *rc = containerof(t, praw_connection_t, task);
  lock(&rc->task.mutex);
  int events = rc->psocket.sched_io_events;
  int fd = rc->psocket.epoll_io.fd;
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
      unlock(&rc->task.mutex);
      return NULL;
    }
    praw_connection_connected_lh(rc);
  }
  unlock(&rc->task.mutex);

  bool wake = false;
  lock(&t->mutex);
  t->working = true;
  if (sched_ready) {
    schedule_done(t);
    if (pni_task_wake_pending(&rc->task)) {
      wake = true;
      pni_task_wake_done(&rc->task);
    }
  }
  unlock(&t->mutex);

  if (wake) pni_raw_wake(&rc->raw_connection);
  if (events & EPOLLIN) pni_raw_read(&rc->raw_connection, fd, rcv, set_error);
  if (events & EPOLLOUT) pni_raw_write(&rc->raw_connection, fd, snd, set_error);
  return &rc->batch;
}

void pni_raw_connection_done(praw_connection_t *rc) {
  bool notify = false;
  bool ready = false;
  lock(&rc->task.mutex);
  pn_proactor_t *p = rc->task.proactor;
  tslot_t *ts = rc->task.runner;
  rc->task.working = false;
  notify = pni_task_wake_pending(&rc->task) && schedule(&rc->task);
  // The task may be in the ready state even if we've got no raw connection
  // wakes outstanding because we dealt with it already in pni_raw_batch_next()
  ready = rc->task.ready;
  unlock(&rc->task.mutex);

  pn_raw_connection_t *raw = &rc->raw_connection;
  int fd = rc->psocket.epoll_io.fd;
  pni_raw_process_shutdown(raw, fd, shutr, shutw);
  int wanted =
    (pni_raw_can_read(raw)  ? EPOLLIN : 0) |
    (pni_raw_can_write(raw) ? EPOLLOUT : 0);
  if (wanted) {
    rc->psocket.epoll_io.wanted = wanted;
    rearm_polling(&rc->psocket.epoll_io, p->epollfd);  // TODO: check for error
  } else {
    bool finished_disconnect = raw->state==conn_fini && !ready && !raw->disconnectpending;
    if (finished_disconnect) {
      // If we're closed and we've sent the disconnect then close
      pni_raw_finalize(raw);
      praw_connection_cleanup(rc);
    }
  }

  lock(&p->sched_mutex);
  notify |= unassign_thread(ts, UNUSED);
  unlock(&p->sched_mutex);
  if (notify) notify_poller(p);
}
