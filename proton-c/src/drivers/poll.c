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

#define _POSIX_C_SOURCE 1

#include <proton/driver.h>
#include "../util.h"
#include "../driver_impl.h"

#include <poll.h>
#include <unistd.h>

typedef struct pn_driver_poller_t {
  size_t capacity;
  struct pollfd *fds;
  size_t nfds;
} pn_driver_poller_t;

typedef struct pn_listener_poller_t {
    int idx;
} pn_listener_poller_t;

typedef struct pn_connector_poller_t {
    int idx;
} pn_connector_poller_t;


int pn_driver_poller_init( struct pn_driver_t *d )
{
    d->poller = calloc(1, sizeof(pn_driver_poller_t));
    if (!d->poller) {
        perror("Unable to allocate poll() driver poller:");
        return -1;
    }
    return 0;
}

void pn_driver_poller_destroy( struct pn_driver_t *d )
{
    if (d->poller) {
        if (d->poller->fds) free(d->poller->fds);
        free(d->poller);
    }
    d->poller = NULL;
}


int pn_listener_poller_init( struct pn_listener_t *l )
{
    l->poller = calloc(1, sizeof(pn_listener_poller_t));
    if (!l->poller) {
        perror("Unable to allocate poll() listener_poller:");
        return -1;
    }
    return 0;
}

void pn_listener_poller_destroy( struct pn_listener_t *l )
{
    if (l->poller) free(l->poller);
    l->poller = NULL;
}


int pn_connector_poller_init( struct pn_connector_t *c )
{
    c->poller = calloc(1, sizeof(pn_connector_poller_t));
    if (!c->poller) {
        perror("Unable to allocate poll() connector_poller:");
        return -1;
    }
    return 0;
}

void pn_connector_poller_destroy( struct pn_connector_t *c )
{
    if (c->poller) free(c->poller);
    c->poller = NULL;
}


void pn_driver_poller_wait(pn_driver_t *d, int timeout)
{
  pn_driver_poller_t *poller = d->poller;
  size_t size = d->listener_count + d->connector_count;
  while (poller->capacity < size + 1) {
    poller->capacity = poller->capacity ? 2*poller->capacity : 16;
    poller->fds = realloc(poller->fds, poller->capacity*sizeof(struct pollfd));
  }

  poller->nfds = 0;

  poller->fds[poller->nfds].fd = d->ctrl[0];
  poller->fds[poller->nfds].events = POLLIN;
  poller->fds[poller->nfds].revents = 0;
  poller->nfds++;

  pn_listener_t *l = d->listener_head;
  for (int i = 0; i < d->listener_count; i++) {
    poller->fds[poller->nfds].fd = l->fd;
    poller->fds[poller->nfds].events = POLLIN;
    poller->fds[poller->nfds].revents = 0;
    l->poller->idx = poller->nfds;
    poller->nfds++;
    l = l->listener_next;
  }

  pn_connector_t *c = d->connector_head;
  for (int i = 0; i < d->connector_count; i++)
  {
    if (!c->closed) {
      poller->fds[poller->nfds].fd = c->fd;
      poller->fds[poller->nfds].events = (c->status & PN_SEL_RD ? POLLIN : 0) |
        (c->status & PN_SEL_WR ? POLLOUT : 0);
      poller->fds[poller->nfds].revents = 0;
      c->poller->idx = poller->nfds;
      poller->nfds++;
    }
    c = c->connector_next;
  }

  DIE_IFE(poll(poller->fds, poller->nfds, d->closed_count > 0 ? 0 : timeout));

  if (poller->fds[0].revents & POLLIN) {
    //clear the pipe
    char buffer[512];
    while (read(d->ctrl[0], buffer, 512) == 512);
  }

  l = d->listener_head;
  while (l) {
    int idx = l->poller->idx;
    l->pending = (idx && poller->fds[idx].revents & POLLIN);
    l = l->listener_next;
  }

  c = d->connector_head;
  while (c) {
    if (c->closed) {
      c->pending_read = false;
      c->pending_write = false;
      c->pending_tick = false;
    } else {
      int idx = c->poller->idx;
      c->pending_read = (idx && poller->fds[idx].revents & POLLIN);
      c->pending_write = (idx && poller->fds[idx].revents & POLLOUT);
    }
    c = c->connector_next;
  }
}
