#ifndef PROTON_CONNECTION_ENGINE_H
#define PROTON_CONNECTION_ENGINE_H

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
 * **Experimental** The connection IO API is a set of functions to simplify
 * integrating proton with different IO and concurrency platforms. The portable
 * parts of a Proton application should use the @ref engine types.  We will
 * use "application" to mean the portable part of the application and
 * "integration" to mean code that integrates with a particular IO platform.
 *
 * The connection_engine functions take a @ref pn_connection_t\*, and perform common
 * tasks involving the @ref pn_connection_t and it's @ref pn_transport_t and
 * @ref pn_collector_t so you can treat them as a unit. You can also work with
 * these types directly for features not available via @ref connection_engine API.
 *
 * @defgroup connection_engine Connection Engine
 *
 * **Experimental**: Toolkit for integrating proton with arbitrary network or IO
 * transports. Provides a single point of control for an AMQP connection and
 * a simple bytes-in/bytes-out interface that lets you:
 *
 * - process AMQP-encoded bytes from some input byte stream
 * - generate @ref pn_event_t events for your application to handle
 * - encode resulting AMQP output bytes for some output byte stream
 *
 * The engine contains a @ref pn_connection_t, @ref pn_transport_t and @ref
 * pn_collector_t and provides functions to operate on all three as a unit for
 * IO integration. You can also use them directly for anything not covered by
 * this API
 *
 * For example a simple blocking IO integration with the imaginary "my_io" library:
 *
 *     pn_connection_engine_t ce;
 *     pn_connection_engine_init(&ce);
 *     while (!pn_connection_engine_finished(&ce) {
 *         // Dispatch events to be handled by the application.
 *         pn_event_t *e;
 *         while ((e = pn_connection_engine_event(&ce))!= NULL) {
 *             my_app_handle(e); // Pass to the application handler
 *             switch (pn_event_type(e)) {
 *                 case PN_CONNECTION_INIT: pn_connection_engine_bind(&ce);
 *                 // Only for full-duplex IO where read/write can shutdown separately.
 *                 case PN_TRANSPORT_CLOSE_READ: my_io_shutdown_read(...); break;
 *                 case PN_TRANSPORT_CLOSE_WRITE: my_io_shutdown_write(...); break;
 *                 default: break;
 *             };
 *             e = pn_connection_engine_pop_event(&ce);
 *         }
 *         // Read from my_io into the connection buffer
 *         pn_rwbytes_t readbuf = pn_connection_engine_read_buffer(&ce);
 *         if (readbuf.size) {
 *             size_t n = my_io_read(readbuf.start, readbuf.size, ...);
 *             if (n > 0) {
 *                 pn_connection_engine_read_done(&ce, n);
 *             } else if (n < 0) {
 *                 pn_connection_engine_errorf(&ce, "read-err", "something-bad (%d): %s", n, ...);
 *                 pn_connection_engine_read_close(&ce);
 *             }
 *         }
 *         // Write from connection buffer to my_io
 *         pn_bytes_t writebuf = pn_connection_engine_write_buffer(&ce);
 *         if (writebuf.size) {
 *             size_t n = my_io_write_data(writebuf.start, writebuf.size, ...);
 *             if (n < 0) {
 *                 pn_connection_engine_errorf(&ce, "write-err", "something-bad (%d): %s", d, ...);
 *                 pn_connection_engine_write_close(&ce);
 *             } else {
 *                 pn_connection_engine_write_done(&ce, n);
 *             }
 *         }
 *     }
 *     // If my_io doesn't have separate read/write shutdown, then we should close it now.
 *     my_io_close(...);
 *
 * AMQP is a full-duplex, asynchronous protocol. The "read" and "write" sides of
 * an AMQP connection can close separately, the example shows how to handle this
 * for full-duplex IO or IO with a simple close.
 *
 * The engine buffers events, you must keep processing till
 * pn_connection_engine_finished() is true, to ensure all reading, writing and event
 * handling (including ERROR and FINAL events) is completely finished.
 *
 * ## Error handling
 *
 * The pn_connection_engine_*() functions do not return an error code. IO errors set
 * the transport condition and are returned as a PN_TRANSPORT_ERROR. The integration
 * code can set errors using pn_connection_engine_errorf()
 *
 * ## Other IO patterns
 *
 * This API supports asynchronous, proactive, non-blocking and reactive IO. An
 * integration does not have to follow the dispatch-read-write sequence above,
 * but note that you should handle all available events before calling
 * pn_connection_engine_read_buffer() and check that `size` is non-zero before
 * starting a blocking or asynchronous read call. A `read` started while there
 * are unprocessed CLOSE events in the buffer may never complete.
 *
 * ## Thread safety
 *
 * The @ref engine types are not thread safe, but each connection and its
 * associated types forms an independent unit. Different connections can be
 * processed concurrently by different threads.
 *
 * @defgroup connection_engine Connection IO
 * @{
 */

#include <proton/import_export.h>
#include <proton/event.h>
#include <proton/types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Struct containing a connection, transport and collector. See
 * pn_connection_engine_init(), pn_connection_engine_destroy() and pn_connection_engine()
 */
typedef struct pn_connection_engine_t {
  pn_connection_t *connection;
  pn_transport_t *transport;
  pn_collector_t *collector;
} pn_connection_engine_t;

/**
 * Set #connection and #transport to the provided values, or create a new
 * @ref pn_connection_t or @ref pn_transport_t if either is NULL.
 * The provided values belong to the connection engine and will be freed by
 * pn_connection_engine_destroy()
 *
 * Create a new @ref pn_collector_t and set as #collector.
 *
 * The transport and connection are *not* bound at this point. You should
 * configure them as needed and let the application handle the
 * PN_CONNECTION_INIT from pn_connection_engine_event() before calling
 * pn_connection_engine_bind().
 *
 * @return if any allocation fails call pn_connection_engine_destroy() and return PN_OUT_OF_MEMORY
 */
PN_EXTERN int pn_connection_engine_init(pn_connection_engine_t*, pn_connection_t*, pn_transport_t*);

/**
 * Bind the connection to the transport when the external IO is ready.
 *
 * The following functions (if called at all) must be called *before* bind:
 * pn_connection_set_username(), pn_connection_set_password(),  pn_transport_set_server()
 *
 * If there is an external IO error during setup, set a transport error, close
 * the transport and then bind. The error events are reported to the application
 * via pn_connection_engine_event().
 *
 * @return an error code if the bind fails.
 */
PN_EXTERN int pn_connection_engine_bind(pn_connection_engine_t *);

/**
 * Unbind, release and free #connection, #transpot and #collector. Set all pointers to NULL.
 * Does not free the @ref pn_connection_engine_t struct itself.
 */
PN_EXTERN void pn_connection_engine_destroy(pn_connection_engine_t *);

/**
 * Get the read buffer.
 *
 * Copy data from your input byte source to buf.start, up to buf.size.
 * Call pn_connection_engine_read_done() when reading is complete.
 *
 * buf.size==0 means reading is not possible: no buffer space or the read side is closed.
 */
PN_EXTERN pn_rwbytes_t pn_connection_engine_read_buffer(pn_connection_engine_t *);

/**
 * Process the first n bytes of data in pn_connection_engine_read_buffer() and
 * reclaim the buffer space.
 */
PN_EXTERN void pn_connection_engine_read_done(pn_connection_engine_t *, size_t n);

/**
 * Close the read side. Call when the IO can no longer be read.
 */
PN_EXTERN void pn_connection_engine_read_close(pn_connection_engine_t *);

/**
 * True if read side is closed.
 */
PN_EXTERN bool pn_connection_engine_read_closed(pn_connection_engine_t *);

/**
 * Get the write buffer.
 *
 * Write data from buf.start to your IO destination, up to a max of buf.size.
 * Call pn_connection_engine_write_done() when writing is complete.
 *
 * buf.size==0 means there is nothing to write.
 */
 PN_EXTERN pn_bytes_t pn_connection_engine_write_buffer(pn_connection_engine_t *);

/**
 * Call when the first n bytes of pn_connection_engine_write_buffer() have been
 * written to IO. Reclaims the buffer space and reset the write buffer.
 */
PN_EXTERN void pn_connection_engine_write_done(pn_connection_engine_t *, size_t n);

/**
 * Close the write side. Call when IO can no longer be written to.
 */
PN_EXTERN void pn_connection_engine_write_close(pn_connection_engine_t *);

/**
 * True if write side is closed.
 */
PN_EXTERN bool pn_connection_engine_write_closed(pn_connection_engine_t *);

/**
 * Close both sides side.
 */
PN_EXTERN void pn_connection_engine_close(pn_connection_engine_t * c);

/**
 * Get the current event. Call pn_connection_engine_done() when done handling it.
 * Note that if PN_TRACE_EVT is enabled this will log the event, so you should
 * avoid calling it more than once per event. Use pn_connection_engine_has_event()
 * to silently test if any events are available.
 *
 * @return NULL if there are no more events ready. Reading/writing data may produce more.
 */
PN_EXTERN pn_event_t* pn_connection_engine_event(pn_connection_engine_t *);

/**
 * True if  pn_connection_engine_event() will return a non-NULL event.
 */
PN_EXTERN bool pn_connection_engine_has_event(pn_connection_engine_t *);

/**
 * Drop the current event, advance pn_connection_engine_event() to the next event.
 */
PN_EXTERN void pn_connection_engine_pop_event(pn_connection_engine_t *);

/**
 * Return true if the the engine is closed for reading and writing and there are
 * no more events.
 *
 * Call pn_connection_engine_free() to free all related memory.
 */
PN_EXTERN bool pn_connection_engine_finished(pn_connection_engine_t *);

/**
 * Set IO error information.
 *
 * The name and formatted description are set on the transport condition, and
 * returned as a PN_TRANSPORT_ERROR event from pn_connection_engine_event().
 *
 * You must call this *before* pn_connection_engine_read_close() or
 * pn_connection_engine_write_close() to ensure the error is processed.
 *
 * If there is already a transport condition set, this call does nothing.  For
 * more complex cases, you can work with the transport condition directly using:
 *
 *     pn_condition_t *cond = pn_transport_condition(pn_connection_transport(conn));
 */
PN_EXTERN void pn_connection_engine_errorf(pn_connection_engine_t *ce, const char *name, const char *fmt, ...);

/**
 * Set IO error information via a va_list, see pn_connection_engine_errorf()
 */
PN_EXTERN void pn_connection_engine_verrorf(pn_connection_engine_t *ce, const char *name, const char *fmt, va_list);

/**
 * Log a string message using the connection's transport log.
 */
PN_EXTERN void pn_connection_engine_log(pn_connection_engine_t *ce, const char *msg);

/**
 * Log a printf formatted message using the connection's transport log.
 */
PN_EXTERN void pn_connection_engine_logf(pn_connection_engine_t *ce, char *fmt, ...);

/**
 * Log a printf formatted message using the connection's transport log.
 */
PN_EXTERN void pn_connection_engine_vlogf(pn_connection_engine_t *ce, const char *fmt, va_list ap);

///@}

#ifdef __cplusplus
}
#endif

#endif // PROTON_CONNECTION_ENGINE_H
