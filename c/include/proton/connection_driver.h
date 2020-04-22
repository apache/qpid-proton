#ifndef PROTON_CONNECTION_DRIVER_H
#define PROTON_CONNECTION_DRIVER_H 1

/*
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
 */

/**
 * @file
 *
 * @copybrief connection_driver
 *
 * @addtogroup connection_driver
 * @{
 *
 * Associate a @ref connection and @ref transport with AMQP byte
 * streams from any source.
 *
 * - process AMQP-encoded bytes from some input byte stream
 * - generate ::pn_event_t events for your application to handle
 * - encode resulting AMQP output bytes for some output byte stream
 *
 * The `pn_connection_driver_*` functions provide a simplified API and
 * extra logic to use ::pn_connection_t and ::pn_transport_t as a
 * unit.  You can also access them directly for features that do not
 * have `pn_connection_driver_*` functions.
 *
 * The driver buffers events and data.  You should run it until
 * pn_connection_driver_finished() is true, to ensure all reading,
 * writing, and event handling (including `ERROR` and `FINAL` events)
 * is finished.
 *
 * ## Error handling
 *
 * The `pn_connection_driver_*` functions do not return an error
 * code. IO errors are set on the transport condition and are returned
 * as a `PN_TRANSPORT_ERROR`. The integration code can set errors
 * using pn_connection_driver_errorf().
 *
 * ## IO patterns
 *
 * This API supports asynchronous, proactive, non-blocking and
 * reactive IO. An integration does not have to follow the
 * dispatch-read-write sequence above, but note that you should handle
 * all available events before calling
 * pn_connection_driver_read_buffer() and check that `size` is
 * non-zero before starting a blocking or asynchronous read call. A
 * `read` started while there are unprocessed `CLOSE` events in the
 * buffer may never complete.
 *
 * AMQP is a full-duplex, asynchronous protocol. The "read" and
 * "write" sides of an AMQP connection can close separately.
 *
 * ## Thread safety
 *
 * The @ref connection_driver types are not thread safe, but each
 * connection and its associated types form an independent
 * unit. Different connections can be processed concurrently by
 * different threads.
 */

#include <proton/import_export.h>
#include <proton/event.h>
#include <proton/types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The elements needed to drive AMQP IO and events.
 */
typedef struct pn_connection_driver_t {
  pn_connection_t *connection;
  pn_transport_t *transport;
  pn_collector_t *collector;
} pn_connection_driver_t;

/**
 * Set connection and transport to the provided values, or create a new
 * @ref pn_connection_t or @ref pn_transport_t if either is NULL.
 * The provided values belong to the connection driver and will be freed by
 * pn_connection_driver_destroy().
 *
 * The transport is bound automatically after the PN_CONNECTION_INIT has been is
 * handled by the application. It can be bound earlier with
 * pn_connection_driver_bind().
 *
 * The following functions must be called before the transport is
 * bound to have effect: pn_connection_set_username(), pn_connection_set_password(),
 * pn_transport_set_server().
 *
 * @return PN_OUT_OF_MEMORY if any allocation fails.
 */
PN_EXTERN int pn_connection_driver_init(pn_connection_driver_t*, pn_connection_t*, pn_transport_t*);

/**
 * Force binding of the transport.  This happens automatically after
 * the PN_CONNECTION_INIT is processed.
 *
 * @return PN_STATE_ERR if the transport is already bound.
 */
PN_EXTERN int pn_connection_driver_bind(pn_connection_driver_t *d);

/**
 * Unbind, release and free the connection and transport. Set all pointers to
 * NULL.  Does not free the @ref pn_connection_driver_t struct itself.
 */
PN_EXTERN void pn_connection_driver_destroy(pn_connection_driver_t *);

/**
 * Disassociate the driver's connection from its transport and collector and
 * sets d->connection = NULL.  Returns the previous value, which must be freed
 * by the caller.
 *
 * The transport and collector are still owned by the driver and will be freed by
 * pn_connection_driver_destroy().
 *
 * @note This has nothing to do with pn_connection_release()
 */
PN_EXTERN pn_connection_t *pn_connection_driver_release_connection(pn_connection_driver_t *d);

/**
 * Get the read buffer.
 *
 * Copy data from your input byte source to buf.start, up to buf.size.
 * Call pn_connection_driver_read_done() when reading is complete.
 *
 * buf.size==0 means reading is not possible: no buffer space or the read side is closed.
 */
PN_EXTERN pn_rwbytes_t pn_connection_driver_read_buffer(pn_connection_driver_t *);

/**
 * Process the first n bytes of data in pn_connection_driver_read_buffer() and
 * reclaim the buffer space.
 */
PN_EXTERN void pn_connection_driver_read_done(pn_connection_driver_t *, size_t n);

/**
 * Close the read side. Call when the IO can no longer be read.
 */
PN_EXTERN void pn_connection_driver_read_close(pn_connection_driver_t *);

/**
 * True if read side is closed.
 */
PN_EXTERN bool pn_connection_driver_read_closed(pn_connection_driver_t *);

/**
 * Get the write buffer.
 *
 * Write data from buf.start to your IO destination, up to a max of buf.size.
 * Call pn_connection_driver_write_done() when writing is complete.
 *
 * buf.size==0 means there is nothing to write.
 */
 PN_EXTERN pn_bytes_t pn_connection_driver_write_buffer(pn_connection_driver_t *);

/**
 * Call when the first n bytes of pn_connection_driver_write_buffer() have been
 * written to IO. Reclaims the buffer space and reset the write buffer.
 */
PN_EXTERN void pn_connection_driver_write_done(pn_connection_driver_t *, size_t n);

/**
 * Close the write side. Call when IO can no longer be written to.
 */
PN_EXTERN void pn_connection_driver_write_close(pn_connection_driver_t *);

/**
 * True if write side is closed.
 */
PN_EXTERN bool pn_connection_driver_write_closed(pn_connection_driver_t *);

/**
 * Close both sides.
 */
PN_EXTERN void pn_connection_driver_close(pn_connection_driver_t * c);

/**
 * Get the next event to handle.
 *
 * @return pointer is valid till the next call of
 * pn_connection_driver_next(). NULL if there are no more events available now,
 * reading/writing may produce more.
 */
PN_EXTERN pn_event_t* pn_connection_driver_next_event(pn_connection_driver_t *);

/**
 * True if  pn_connection_driver_next_event() will return a non-NULL event.
 */
PN_EXTERN bool pn_connection_driver_has_event(pn_connection_driver_t *);

/**
 * Return true if the the driver is closed for reading and writing and there are
 * no more events.
 *
 * Call pn_connection_driver_destroy() to free all related memory.
 */
PN_EXTERN bool pn_connection_driver_finished(pn_connection_driver_t *);

/**
 * Set transport error.
 *
 * The name and formatted description are set on the transport condition, and
 * returned as a PN_TRANSPORT_ERROR event from pn_connection_driver_next_event().
 *
 * You must call this *before* pn_connection_driver_read_close() or
 * pn_connection_driver_write_close() to ensure the error is processed.
 */
PN_EXTERN void pn_connection_driver_errorf(pn_connection_driver_t *d, const char *name, const char *fmt, ...);

/**
 * Set transport error via a va_list, see pn_connection_driver_errorf()
 */
PN_EXTERN void pn_connection_driver_verrorf(pn_connection_driver_t *d, const char *name, const char *fmt, va_list);

/**
 * The write side of the transport is closed, it will no longer produce bytes to write to
 * external IO. Synonym for PN_TRANSPORT_HEAD_CLOSED
 */
#define PN_TRANSPORT_WRITE_CLOSED PN_TRANSPORT_HEAD_CLOSED

/**
 * The read side of the transport is closed, it will no longer read bytes from external
 * IO. Alias for PN_TRANSPORT_TAIL_CLOSED
 */
#define PN_TRANSPORT_READ_CLOSED PN_TRANSPORT_TAIL_CLOSED

/**
 * **Deprecated** - Use pn_transport_log().
 */
PN_EXTERN void pn_connection_driver_log(pn_connection_driver_t *d, const char *msg);

/**
 * **Deprecated** - Use pn_transport_logf().
 */
PN_EXTERN void pn_connection_driver_logf(pn_connection_driver_t *d, const char *fmt, ...);

/**
 * **Deprecated** - Use pn_transport_vlogf().
 */
PN_EXTERN void pn_connection_driver_vlogf(pn_connection_driver_t *d, const char *fmt, va_list ap);

/**
 * Associate a pn_connection_t with its pn_connection_driver_t.
 *
 * **NOTE**: this is only for use by IO integration writers. If you are using the standard
 * pn_proactor_t you *must not* use this function.
 *
 * @return pointer to the pn_connection_driver_t* field in a pn_connection_t.
 *
 * Return type is pointer to a pointer so that the caller can (if desired) use
 * atomic operations when loading and storing the value.
 */
PN_EXTERN pn_connection_driver_t **pn_connection_driver_ptr(pn_connection_t *connection);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* connection_driver.h */
