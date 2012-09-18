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
#include "../driver-internal.h"

#include <sys/select.h>
#include <unistd.h>

typedef struct pn_driver_poller_t {
  fd_set readfds;
  fd_set writefds;
  int max_fds;
} pn_driver_poller_t;

// pn_listener_poller_t not used
// pn_connector_poller_t not used




int pn_driver_poller_init( struct pn_driver_t *d )
{
    d->poller = calloc(1, sizeof(pn_driver_poller_t));
    if (!d->poller) {
        perror("Unable to allocate select() driver_poller:");
        return -1;
    }
    return 0;
}

void pn_driver_poller_destroy( struct pn_driver_t *d )
{
    if (d->poller) free(d->poller);
    d->poller = NULL;
}


int pn_listener_poller_init( struct pn_listener_t *l )
{
    l->poller = NULL; // not used
    return 0;
}

void pn_listener_poller_destroy( struct pn_listener_t *l )
{
}


int pn_connector_poller_init( struct pn_connector_t *c )
{
    c->poller = NULL; // not used
    return 0;
}

void pn_connector_poller_destroy( struct pn_connector_t *c )
{
}


void pn_driver_poller_wait(pn_driver_t *d, int timeout)
{
  pn_driver_poller_t *poller = d->poller;

  // setup the select
  FD_ZERO(&poller->readfds);
  FD_ZERO(&poller->writefds);

  FD_SET(d->ctrl[0], &poller->readfds);
  poller->max_fds = d->ctrl[0];

  pn_listener_t *l = d->listener_head;
  for (int i = 0; i < d->listener_count; i++) {
      FD_SET(l->fd, &poller->readfds);
      if (l->fd > poller->max_fds) poller->max_fds = l->fd;
      l = l->listener_next;
  }

  pn_connector_t *c = d->connector_head;
  for (int i = 0; i < d->connector_count; i++) {
      if (!c->closed && (c->status & (PN_SEL_RD|PN_SEL_WR))) {
          if (c->status & PN_SEL_RD)
              FD_SET(c->fd, &poller->readfds);
          if (c->status & PN_SEL_WR)
              FD_SET(c->fd, &poller->writefds);
          if (c->fd > poller->max_fds) poller->max_fds = c->fd;
      }
      c = c->connector_next;
  }

  struct timeval to = {0};
  if (timeout > 0) {
      // convert millisecs to sec and usec:
      to.tv_sec = timeout/1000;
      to.tv_usec = (timeout - (to.tv_sec * 1000)) * 1000;
  }

  int nfds = select(poller->max_fds + 1, &poller->readfds, &poller->writefds, NULL, timeout < 0 ? NULL : &to);
  DIE_IFE(nfds);

  if (nfds > 0) {

      if (FD_ISSET(d->ctrl[0], &poller->readfds)) {
          //clear the pipe
          char buffer[512];
          while (read(d->ctrl[0], buffer, 512) == 512);
      }

      pn_listener_t *l = d->listener_head;
      while (l) {
          l->pending = FD_ISSET(l->fd, &poller->readfds);
          l = l->listener_next;
      }

      pn_connector_t *c = d->connector_head;
      while (c) {
          if (c->closed) {
              c->pending_read = false;
              c->pending_write = false;
              c->pending_tick = false;
          } else {
              c->pending_read = FD_ISSET(c->fd, &poller->readfds);
              c->pending_write = FD_ISSET(c->fd, &poller->writefds);
          }
          c = c->connector_next;
      }
  }
}
