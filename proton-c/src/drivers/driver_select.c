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

#include <sys/select.h>
#include <unistd.h>

typedef struct pn_driver_impl_t {
  fd_set readfds;
  fd_set writefds;
  int max_fds;
} pn_driver_impl_t;

// pn_listener_impl_t not used
// pn_connector_impl_t not used




int pn_driver_impl_init( struct pn_driver_t *d )
{
    d->impl = calloc(1, sizeof(pn_driver_impl_t));
    if (!d->impl) {
        perror("Unable to allocate select() driver_impl:");
        return -1;
    }
    return 0;
}

void pn_driver_impl_destroy( struct pn_driver_t *d )
{
    if (d->impl) free(d->impl);
    d->impl = NULL;
}


int pn_listener_impl_init( struct pn_listener_t *l )
{
    l->impl = NULL; // not used
    return 0;
}

void pn_listener_impl_destroy( struct pn_listener_t *l )
{
}


int pn_connector_impl_init( struct pn_connector_t *c )
{
    c->impl = NULL; // not used
    return 0;
}

void pn_connector_impl_destroy( struct pn_connector_t *c )
{
}


void pn_driver_impl_wait(pn_driver_t *d, int timeout)
{
  pn_driver_impl_t *impl = d->impl;

  // setup the select
  impl->max_fds = -1;
  FD_ZERO(&impl->readfds);
  FD_ZERO(&impl->writefds);

  FD_SET(d->ctrl[0], &impl->readfds);
  if (d->ctrl[0] > impl->max_fds) impl->max_fds = d->ctrl[0];

  pn_listener_t *l = d->listener_head;
  for (int i = 0; i < d->listener_count; i++) {
      FD_SET(l->fd, &impl->readfds);
      if (l->fd > impl->max_fds) impl->max_fds = l->fd;
      l = l->listener_next;
  }

  pn_connector_t *c = d->connector_head;
  for (int i = 0; i < d->connector_count; i++) {
      if (!c->closed && (c->status & (PN_SEL_RD|PN_SEL_WR))) {
          if (c->status & PN_SEL_RD)
              FD_SET(c->fd, &impl->readfds);
          if (c->status & PN_SEL_WR)
              FD_SET(c->fd, &impl->writefds);
          if (c->fd > impl->max_fds) impl->max_fds = c->fd;
      }
      c = c->connector_next;
  }

  struct timeval to = {0};
  if (timeout > 0) {
      // convert millisecs to sec and usec:
      to.tv_sec = timeout/1000;
      to.tv_usec = (timeout - (to.tv_sec * 1000)) * 1000;
  }

  int nfds = select(impl->max_fds + 1, &impl->readfds, &impl->writefds, NULL, timeout < 0 ? NULL : &to);
  DIE_IFE(nfds);

  if (nfds > 0) {

      if (FD_ISSET(d->ctrl[0], &impl->readfds)) {
          //clear the pipe
          char buffer[512];
          while (read(d->ctrl[0], buffer, 512) == 512);
      }

      pn_listener_t *l = d->listener_head;
      while (l) {
          l->pending = FD_ISSET(l->fd, &impl->readfds);
          l = l->listener_next;
      }

      pn_connector_t *c = d->connector_head;
      while (c) {
          if (c->closed) {
              c->pending_read = false;
              c->pending_write = false;
              c->pending_tick = false;
          } else {
              c->pending_read = FD_ISSET(c->fd, &impl->readfds);
              c->pending_write = FD_ISSET(c->fd, &impl->writefds);
          }
          c = c->connector_next;
      }
  }
}
