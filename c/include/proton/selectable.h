#ifndef PROTON_SELECTABLE_H
#define PROTON_SELECTABLE_H 1

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

#include <proton/import_export.h>
#include <proton/object.h>
#include <proton/event.h>
#include <proton/type_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @cond INTERNAL
 */

/**
 * A ::pn_socket_t provides an abstract handle to an IO stream.  The
 * pipe version is uni-directional.  The network socket version is
 * bi-directional.  Both are non-blocking.
 *
 * pn_socket_t handles from pn_pipe() may only be used with
 * pn_read(), pn_write(), pn_close() and pn_selector_select().
 *
 * pn_socket_t handles from pn_listen(), pn_accept() and
 * pn_connect() must perform further IO using Proton functions.
 * Mixing Proton io.h functions with native IO functions on the same
 * handles will result in undefined behavior.
 *
 * pn_socket_t handles may only be used with a single pn_io_t during
 * their lifetime.
 */
#if defined(_WIN32) && ! defined(__CYGWIN__)
#ifdef _WIN64
typedef unsigned __int64 pn_socket_t;
#else
typedef unsigned int pn_socket_t;
#endif
#define PN_INVALID_SOCKET (pn_socket_t)(~0)
#else
typedef int pn_socket_t;
#define PN_INVALID_SOCKET (-1)
#endif

/**
 * A selectable object provides an interface that can be used to
 * incorporate proton's I/O into third party event loops.
 *
 * Every selectable is associated with exactly one file descriptor.
 * Selectables may be interested in three kinds of events, read
 * events, write events, and timer events. 
 *
 * When a read, write, or timer event occurs, the selectable must be
 * notified by calling ::pn_selectable_readable(),
 * ::pn_selectable_writable(), and ::pn_selectable_expired() as
 * appropriate.
 *
 * Once a selectable reaches a terminal state (see
 * ::pn_selectable_is_terminal()), it will never be interested in
 * events of any kind. When this occurs it should be removed from the
 * external event loop and discarded using ::pn_selectable_free().
 */
typedef struct pn_selectable_t pn_selectable_t;

PNX_EXTERN pn_selectable_t *pn_selectable(void);

PNX_EXTERN void pn_selectable_on_readable(pn_selectable_t *sel, void (*readable)(pn_selectable_t *));
PNX_EXTERN void pn_selectable_on_writable(pn_selectable_t *sel, void (*writable)(pn_selectable_t *));
PNX_EXTERN void pn_selectable_on_expired(pn_selectable_t *sel, void (*expired)(pn_selectable_t *));
PNX_EXTERN void pn_selectable_on_error(pn_selectable_t *sel, void (*error)(pn_selectable_t *));
PNX_EXTERN void pn_selectable_on_release(pn_selectable_t *sel, void (*release)(pn_selectable_t *));
PNX_EXTERN void pn_selectable_on_finalize(pn_selectable_t *sel, void (*finalize)(pn_selectable_t *));

PNX_EXTERN pn_record_t *pn_selectable_attachments(pn_selectable_t *sel);

/**
 * Get the file descriptor associated with a selectable.
 *
 * @param[in] selectable a selectable object
 * @return the file descriptor associated with the selectable
 */
PNX_EXTERN pn_socket_t pn_selectable_get_fd(pn_selectable_t *selectable);

/**
 * Set the file descriptor associated with a selectable.
 *
 * @param[in] selectable a selectable object
 * @param[in] fd the file descriptor
 */
PNX_EXTERN void pn_selectable_set_fd(pn_selectable_t *selectable, pn_socket_t fd);

/**
 * Check if a selectable is interested in readable events.
 *
 * @param[in] selectable a selectable object
 * @return true iff the selectable is interested in read events
 */
PNX_EXTERN bool pn_selectable_is_reading(pn_selectable_t *selectable);

PNX_EXTERN void pn_selectable_set_reading(pn_selectable_t *sel, bool reading);

/**
 * Check if a selectable is interested in writable events.
 *
 * @param[in] selectable a selectable object
 * @return true iff the selectable is interested in writable events
 */
PNX_EXTERN bool pn_selectable_is_writing(pn_selectable_t *selectable);

  PNX_EXTERN void pn_selectable_set_writing(pn_selectable_t *sel, bool writing);

/**
 * Get the next deadline for a selectable.
 *
 * A selectable with a deadline is interested in being notified when
 * that deadline expires. Zero indicates there is currently no
 * deadline.
 *
 * @param[in] selectable a selectable object
 * @return the next deadline or zero
 */
PNX_EXTERN pn_timestamp_t pn_selectable_get_deadline(pn_selectable_t *selectable);

PNX_EXTERN void pn_selectable_set_deadline(pn_selectable_t *sel, pn_timestamp_t deadline);

/**
 * Notify a selectable that the file descriptor is readable.
 *
 * @param[in] selectable a selectable object
 */
PNX_EXTERN void pn_selectable_readable(pn_selectable_t *selectable);

/**
 * Notify a selectable that the file descriptor is writable.
 *
 * @param[in] selectable a selectable object
 */
PNX_EXTERN void pn_selectable_writable(pn_selectable_t *selectable);

/**
 * Notify a selectable that there is an error on the file descriptor.
 *
 * @param[in] selectable a selectable object
 */
PNX_EXTERN void pn_selectable_error(pn_selectable_t *selectable);

/**
 * Notify a selectable that its deadline has expired.
 *
 * @param[in] selectable a selectable object
 */
PNX_EXTERN void pn_selectable_expired(pn_selectable_t *selectable);

/**
 * Check if a selectable is registered.
 *
 * This flag is set via ::pn_selectable_set_registered() and can be
 * used for tracking whether a given selectable has been registered
 * with an external event loop.
 *
 * @param[in] selectable
 * @return true if the selectable is registered
 */
PNX_EXTERN bool pn_selectable_is_registered(pn_selectable_t *selectable);

/**
 * Set the registered flag for a selectable.
 *
 * See ::pn_selectable_is_registered() for details.
 *
 * @param[in] selectable a selectable object
 * @param[in] registered the registered flag
 */
PNX_EXTERN void pn_selectable_set_registered(pn_selectable_t *selectable, bool registered);

/**
 * Check if a selectable is in the terminal state.
 *
 * A selectable that is in the terminal state will never be interested
 * in being notified of events of any kind ever again. Once a
 * selectable reaches this state it should be removed from any
 * external I/O loops and freed in order to reclaim any resources
 * associated with it.
 *
 * @param[in] selectable a selectable object
 * @return true if the selectable is in the terminal state, false otherwise
 */
PNX_EXTERN bool pn_selectable_is_terminal(pn_selectable_t *selectable);

/**
 * Terminate a selectable.
 *
 * @param[in] selectable a selectable object
 */
PNX_EXTERN void pn_selectable_terminate(pn_selectable_t *selectable);

PNX_EXTERN void pn_selectable_release(pn_selectable_t *selectable);

/**
 * Free a selectable object.
 *
 * @param[in] selectable a selectable object (or NULL)
 */
PNX_EXTERN void pn_selectable_free(pn_selectable_t *selectable);

/**
 * Configure a selectable with a set of callbacks that emit readable,
 * writable, and expired events into the supplied collector.
 *
 * @param[in] selectable a selectable object
 * @param[in] collector a collector object
 */
PNX_EXTERN void pn_selectable_collect(pn_selectable_t *selectable, pn_collector_t *collector);

/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* selectable.h */
