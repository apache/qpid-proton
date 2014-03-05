#ifndef PROTON_TRANSPORT_H
#define PROTON_TRANSPORT_H 1

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
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * Transport API for the proton Engine.
 *
 * @defgroup transport Transport
 * @ingroup engine
 * @{
 */

typedef int pn_trace_t;
typedef void (*pn_tracer_t)(pn_transport_t *transport, const char *message);

#define PN_TRACE_OFF (0)
#define PN_TRACE_RAW (1)
#define PN_TRACE_FRM (2)
#define PN_TRACE_DRV (4)

/** Factory for creating a transport.
 *
 * A transport to be used by a connection to interface with the
 * network. There can only be one connection associated with a
 * transport. See pn_transport_bind().
 *
 * @return pointer to new transport
 */
PN_EXTERN pn_transport_t *pn_transport(void);

/** Binds the transport to an AMQP connection endpoint.
 *
 * @return an error code, or 0 on success
 */

PN_EXTERN int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection);

PN_EXTERN int pn_transport_unbind(pn_transport_t *transport);

PN_EXTERN pn_error_t *pn_transport_error(pn_transport_t *transport);
/* deprecated */
PN_EXTERN ssize_t pn_transport_input(pn_transport_t *transport, const char *bytes, size_t available);
/* deprecated */
PN_EXTERN ssize_t pn_transport_output(pn_transport_t *transport, char *bytes, size_t size);

/** Report the amount of free space for input following the
 * transport's tail pointer. If the engine is in an exceptional state
 * such as encountering an error condition or reaching the end of
 * stream state, a negative value will be returned indicating the
 * condition. If an error is indicated, futher details can be obtained
 * from ::pn_transport_error. Calls to ::pn_transport_process may
 * alter the value of this pointer. See ::pn_transport_process for
 * details.
 *
 * @param[in] transport the transport
 * @return the free space in the transport, PN_EOS or error code if < 0
 */
PN_EXTERN ssize_t pn_transport_capacity(pn_transport_t *transport);

/** Return the transport's tail pointer. The amount of free space
 * following this pointer is reported by ::pn_transport_capacity.
 * Calls to ::pn_transport_process may alther the value of this
 * pointer. See ::pn_transport_process for details.
 *
 * @param[in] transport the transport
 * @return a pointer to the transport's input buffer, NULL if no capacity available.
 */
PN_EXTERN char *pn_transport_tail(pn_transport_t *transport);

/** Pushes the supplied bytes into the tail of the transport. This is
 * equivalent to copying @c size bytes afther the tail pointer and
 * then calling ::pn_transport_process with an argument of @c size. It
 * is an error to call this with a @c size larger than the capacity
 * reported by ::pn_transport_capacity.
 *
 * @param[in] transport the transport
 * @param[in] src the start of the data to push into the transport
 * @param[in] size the amount of data to push into the transport
 *
 * @return 0 on success, or error code if < 0
 */
PN_EXTERN int pn_transport_push(pn_transport_t *transport, const char *src, size_t size);

/** Process input data following the tail pointer. Calling this
 * function will cause the transport to consume @c size bytes of input
 * occupying the free space following the tail pointer. Calls to this
 * function may change the value of ::pn_transport_tail, as well as
 * the amount of free space reported by ::pn_transport_capacity.
 *
 * @param[in] transport the transport
 * @param[in] size the amount of data written to the transport's input buffer
 * @return 0 on success, or error code if < 0
 */
PN_EXTERN int pn_transport_process(pn_transport_t *transport, size_t size);

/** Indicate that the input has reached End Of Stream (EOS).  This
 * tells the transport that no more input will be forthcoming.
 *
 * @param[in] transport the transport
 * @return 0 on success, or error code if < 0
 */
PN_EXTERN int pn_transport_close_tail(pn_transport_t *transport);

/** Report the number of pending output bytes following the
 * transport's head pointer. If the engine is in an exceptional state
 * such as encountering an error condition or reaching the end of
 * stream state, a negative value will be returned indicating the
 * condition. If an error is indicated, further details can be
 * obtained from ::pn_transport_error. Calls to ::pn_transport_pop may
 * alter the value of this pointer. See ::pn_transport_pop for
 * details.
 *
 * @param[in] transport the transport
 * @return the number of pending output bytes, or an error code
 */
PN_EXTERN ssize_t pn_transport_pending(pn_transport_t *transport);

/** Return the transport's head pointer. This pointer references
 * queued output data. The ::pn_transport_pending function reports how
 * many bytes of output data follow this pointer. Calls to
 * ::pn_transport_pop may alter this pointer and any data it
 * references. See ::pn_transport_pop for details.
 *
 * @param[in] transport the transport
 * @return a pointer to the transport's output buffer, or NULL if no pending output.
 */
PN_EXTERN const char *pn_transport_head(pn_transport_t *transport);

/** Copies @c size bytes from the head of the transport to the @c dst
 * pointer. It is an error to call this with a value of @c size that
 * is greater than the value reported by ::pn_transport_pending.
 *
 * @param[in] transport the transport
 * @param[out] dst the destination buffer
 * @param[in] size the capacity of the destination buffer
 * @return 0 on success, or error code if < 0
 */
PN_EXTERN int pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);

/** Removes @c size bytes of output from the pending output queue
 * following the transport's head pointer. Calls to this function may
 * alter the transport's head pointer as well as the number of pending
 * bytes reported by ::pn_transport_pending.
 *
 * @param[in] transport the transport
 * @param[in] size the number of bytes to remove
 */
PN_EXTERN void pn_transport_pop(pn_transport_t *transport, size_t size);

/** Indicate that the output has closed.  This tells the transport
 * that no more output will be popped.
 *
 * @param[in] transport the transport
 * @return 0 on success, or error code if < 0
 */
PN_EXTERN int pn_transport_close_head(pn_transport_t *transport);


/** Process any pending transport timer events.
 *
 * This method should be called after all pending input has been processed by the
 * transport (see ::pn_transport_input), and before generating output (see
 * ::pn_transport_output).  It returns the deadline for the next pending timer event, if
 * any are present.
 *
 * @param[in] transport the transport to process.
 * @param[in] now the current time
 *
 * @return if non-zero, then the expiration time of the next pending timer event for the
 * transport.  The caller must invoke pn_transport_tick again at least once at or before
 * this deadline occurs.
 */
PN_EXTERN pn_timestamp_t pn_transport_tick(pn_transport_t *transport, pn_timestamp_t now);
PN_EXTERN void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace);
PN_EXTERN void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
PN_EXTERN pn_tracer_t pn_transport_get_tracer(pn_transport_t *transport);
PN_EXTERN void pn_transport_set_context(pn_transport_t *transport, void *context);
PN_EXTERN void *pn_transport_get_context(pn_transport_t *transport);
PN_EXTERN void pn_transport_log(pn_transport_t *transport, const char *message);
PN_EXTERN void pn_transport_logf(pn_transport_t *transport, const char *fmt, ...);

PN_EXTERN uint16_t pn_transport_get_channel_max(pn_transport_t *transport);
PN_EXTERN void pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max);
PN_EXTERN uint16_t pn_transport_remote_channel_max(pn_transport_t *transport);

// max frame of zero means "unlimited"
PN_EXTERN uint32_t pn_transport_get_max_frame(pn_transport_t *transport);
PN_EXTERN void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size);
PN_EXTERN uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport);

/* timeout of zero means "no timeout" */
PN_EXTERN pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport);
PN_EXTERN void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout);
PN_EXTERN pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport);

PN_EXTERN uint64_t pn_transport_get_frames_output(const pn_transport_t *transport);
PN_EXTERN uint64_t pn_transport_get_frames_input(const pn_transport_t *transport);
PN_EXTERN bool pn_transport_quiesced(pn_transport_t *transport);
PN_EXTERN bool pn_transport_closed(pn_transport_t *transport);
PN_EXTERN void pn_transport_free(pn_transport_t *transport);

#ifdef __cplusplus
}
#endif

/** @}
 */

#endif /* transport.h */
