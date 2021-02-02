#ifndef PROTON_RAW_CONNECTION_H
#define PROTON_RAW_CONNECTION_H 1

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

#include <proton/condition.h>
#include <proton/event.h>
#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @addtogroup raw_connection
 * @{
 */

/**
 * A descriptor used to represent a single raw buffer in memory.
 *
 * @note The intent of the offset is to allow the actual bytes being read/written to be at a variable
 * location relative to the head of the buffer because of other data or structures that are important to the application
 * associated with the data to be written but not themselves read/written to the connection.
 *
 * @note For read buffers: When read buffers are returned to the application size will be the number of bytes read.
 * Read operations will not change the context, bytes or capacity members of the structure.
 *
 * @note For write buffers: When write buffers are returned to the application all of the struct members will be returned
 * unaltered. Also write operations will not modify the bytes of the buffer passed in at all. In principle this means that
 * the write buffer can be used for multiple writes at the same time as long as the actual buffer is unmodified by the
 * application at any time the buffer is being used by any raw connection.
 */
typedef struct pn_raw_buffer_t {
  uintptr_t context; /**< Used to associate arbitrary application data with this raw buffer */
  char *bytes; /**< Pointer to the start of the raw buffer, if this is null then no buffer is represented */
  uint32_t capacity; /**< Count of available bytes starting at @ref bytes */
  uint32_t size; /**< Number of bytes read or to be written starting at @ref offset */
  uint32_t offset; /**< First byte in the buffer to be read or written */
} pn_raw_buffer_t;


/**
 * Create a new raw connection for use with the @ref proactor.
 * See @ref pn_proactor_raw_connect and @ref pn_listener_raw_accept.
 *
 * @return A newly allocated pn_raw_connection_t or NULL if there wasn't sufficient memory.
 *
 * @note This is the only pn_raw_connection_t function that allocates memory. So an application that
 * wants good control of out of memory conditions should check the return value for NULL.
 *
 * @note It would be a good practice is to create a raw connection and attach application
 * specific context to it before giving it to the proactor.
 *
 * @note There is no way to free a pn_raw_connection_t as once passed to the proactor the proactor owns
 * it and controls its lifecycle.
 */
PNP_EXTERN pn_raw_connection_t *pn_raw_connection(void);

/**
 * Get the local address of a raw connection. Return `NULL` if not available.
 * Pointer is invalid after the transport closes (@ref PN_RAW_CONNECTION_DISCONNECTED event is handled)
 */
PNP_EXTERN const struct pn_netaddr_t *pn_raw_connection_local_addr(pn_raw_connection_t *connection);

/**
 * Get the local address of a raw connection. Return `NULL` if not available.
 * Pointer is invalid after the transport closes (@ref PN_RAW_CONNECTION_DISCONNECTED event is handled)
 */
PNP_EXTERN const struct pn_netaddr_t *pn_raw_connection_remote_addr(pn_raw_connection_t *connection);

/**
 * Close a raw connection.
 * This will flush any buffers to be written; close the underlying socket and release all buffers held by the
 * raw connection.
 *
 * It will cause @ref PN_RAW_CONNECTION_READ and @ref PN_RAW_CONNECTION_WRITTEN to be emitted so
 * the application can clean up buffers given to the raw connection. After that a
 * @ref PN_RAW_CONNECTION_DISCONNECTED event will be emitted to allow the application to clean up
 * any other state held by the raw connection.
 *
 */
PNP_EXTERN void pn_raw_connection_close(pn_raw_connection_t *connection);

/**
 * Shutdown a raw connection for reading.
 * This will close the underlying socket for reading and release all empty read buffers held by the raw connection.
 *
 * It will cause @ref PN_RAW_CONNECTION_READ to be emitted so the application can clean up buffers given to the raw
 * connection. Note that these buffers may still also contain data read from the socket but not yet consumed by the
 * application.
 *
 * If @ref pn_raw_connection_write_close() has already been called then @ref PN_RAW_CONNECTION_DISCONNECTED will then
 * also be emitted.
 *
 * In order to fully close a raw connection the application will need to either call @ref pn_raw_connection_close()
 * or @ref pn_raw_connection_write_close() after it calls @ref pn_raw_connection_read_close().
 */
PNP_EXTERN void pn_raw_connection_read_close(pn_raw_connection_t *connection);

/**
 * Shutdown a raw connection for writing.
 * This will flush any buffers to be written to the socket; close the underlying socket for writing and release all
 * write buffers held by the raw connection.
 *
 * It will cause @ref PN_RAW_CONNECTION_WRITTEN to be emitted so the application can clean up write buffers given to
 * the raw connection.
 *
 * If @ref pn_raw_connection_read_close() has already been called then @ref PN_RAW_CONNECTION_DISCONNECTED will then
 * also be emitted.
 *
 * In order to fully close a raw connection the application will need to either call @ref pn_raw_connection_close()
 * or @ref pn_raw_connection_read_close() after it calls @ref pn_raw_connection_write_close().
 */
PNP_EXTERN void pn_raw_connection_write_close(pn_raw_connection_t *connection);

/**
 * Query the raw connection for how many more read buffers it can be given
 */
PNP_EXTERN size_t pn_raw_connection_read_buffers_capacity(pn_raw_connection_t *connection);

/**
 * Query the raw connection for how many more write buffers it can be given
 */
PNP_EXTERN size_t pn_raw_connection_write_buffers_capacity(pn_raw_connection_t *connection);

/**
 * Give the raw connection buffers to use for reading from the underlying socket.
 * If the raw socket has no read buffers then the application will never receive
 * a @ref PN_RAW_CONNECTION_READ event.
 *
 * A @ref PN_RAW_CONNECTION_NEED_READ_BUFFERS event will be generated immediately after
 * the @ref PN_RAW_CONNECTION_CONNECTED event if there are no read buffers. It will also be
 * generated whenever the raw connection runs out of read buffers. In both these cases the
 * event will not be generated again until @ref pn_raw_connection_give_read_buffers is called.
 *
 * @return the number of buffers actually given to the raw connection. This will only be different
 * from the number supplied if the connection has no more space to record more buffers. In this case
 * the buffers taken will be the earlier buffers in the array supplied, the elements 0 to the
 * returned value-1.
 *
 * @note The buffers given to the raw connection are owned by it until the application
 * receives the buffers back with a @ref pn_raw_connection_take_read_buffers call. They must
 * not be accessed at all (written or even read) from calling @ref pn_raw_connection_give_read_buffers
 * until receiving them back.
 *
 * @note The application should not assume that the @ref PN_RAW_CONNECTION_NEED_READ_BUFFERS
 * event signifies that the connection is readable.
 */
PNP_EXTERN size_t pn_raw_connection_give_read_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t const *buffers, size_t num);

/**
 * Fetch buffers with bytes read from the raw socket
 *
 * The buffers will be placed in the buffers array in the order in which they were read. So the first buffer in the array will contain the
 * first bytes read; the second buffer the next bytes etc.
 *
 * @param[out] buffers pointer to an array of @ref pn_raw_buffer_t structures which will be filled in with the read buffer information
 * @param[in] num the number of buffers allocated in the passed in array of buffers
 * @return the number of buffers being returned, if there are no read bytes then this will be 0. As many buffers will be returned
 * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
 * read. But if the number is the same there may be more read buffers to take.
 *
 * @note After the application receives @ref PN_RAW_CONNECTION_READ there should be bytes read from the socket and
 * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_connection_take_read_buffers
 * until it returns 0.
 */
PNP_EXTERN size_t pn_raw_connection_take_read_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t *buffers, size_t num);

/**
 * Give the raw connection buffers to write to the underlying socket.
 *
 * The buffers will be written to the connection in the order that they are passed in. That is the first buffer in the array of buffers
 * passed will be the first buffer written to the connection; the second buffer passed in will be the second written etc.
 *
 * A @ref PN_RAW_CONNECTION_WRITTEN event will be generated once the buffers have been written to the socket
 * until this point the buffers must not be accessed at all (written or even read).
 *
 * A @ref PN_RAW_CONNECTION_NEED_WRITE_BUFFERS event will be generated immediately after
 * the @ref PN_RAW_CONNECTION_CONNECTED event if there are no write buffers. It will also be
 * generated whenever the raw connection finishes writing all the write buffers. In both these cases the
 * event will not be generated again until @ref pn_raw_connection_write_buffers is called.
 *
 * @return the number of buffers actually recorded by the raw connection to write. This will only be different
 * from the number supplied if the connection has no more space to record more buffers. In this case
 * the buffers recorded will be the earlier buffers in the array supplied, the elements 0 to the
 * returned value-1.
 *
 */
PNP_EXTERN size_t pn_raw_connection_write_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t const *buffers, size_t num);

/**
 * Return a buffer chain with buffers that have all been written to the raw socket
 *
 * @param[out] buffers pointer to an array of @ref pn_raw_buffer_t structures which will be filled in with the written buffer information
 * @param[in] num the number of buffers allocated in the passed in array of buffers
 * @return the number of buffers being returned, if there is are no written buffers to return then this will be 0. As many buffers will be returned
 * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
 * written. But if the number is the same there may be more written buffers to take.
 *
 * @note After the application receives @ref PN_RAW_CONNECTION_WRITTEN there should be bytes written to the socket and
 * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_connection_take_written_buffers
 * until it returns 0.
 *
 * @note The buffers will be returned in the same order as they were originally passed in.
 */
PNP_EXTERN size_t pn_raw_connection_take_written_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t *buffers, size_t num);

/**
 * Is @p connection closed for read?
 *
 * @return true if the raw connection is closed for read.
 */
PNP_EXTERN bool pn_raw_connection_is_read_closed(pn_raw_connection_t *connection);

/**
 * Is @p connection closed for write?
 *
 * @return true if the raw connection is closed for write.
 */
PNP_EXTERN bool pn_raw_connection_is_write_closed(pn_raw_connection_t *connection);

/**
 * Return a @ref PN_RAW_CONNECTION_WAKE event for @p connection as soon as possible.
 *
 * At least one wake event will be returned, serialized with other @ref proactor_events
 * for the same raw connection.  Wakes can be "coalesced" - if several
 * @ref pn_raw_connection_wake() calls happen close together, there may be only one
 * @ref PN_RAW_CONNECTION_WAKE event that occurs after all of them.
 *
 * @note Thread-safe
 */
PNP_EXTERN void pn_raw_connection_wake(pn_raw_connection_t *connection);

/**
 * Get additional information about a raw connection error.
 * There is a raw connection error if the @ref PN_RAW_CONNECTION_DISCONNECTED event
 * is received and the pn_condition_t associated is non null (@see pn_condition_is_set).
 *
 * The value returned is only valid until the end of handler for the
 * @ref PN_RAW_CONNECTION_DISCONNECTED event.
 */
PNP_EXTERN pn_condition_t *pn_raw_connection_condition(pn_raw_connection_t *connection);

/**
 * Get the application context associated with this raw connection.
 *
 * The application context for a raw connection may be set using
 * ::pn_raw_connection_set_context.
 *
 * @param[in] connection the raw connection whose context is to be returned.
 * @return the application context for the raw connection
 */
PNP_EXTERN void *pn_raw_connection_get_context(pn_raw_connection_t *connection);

/**
 * Set a new application context for a raw connection.
 *
 * The application context for a raw connection may be retrieved
 * using ::pn_raw_connection_get_context.
 *
 * @param[in] connection the raw connection object
 * @param[in] context the application context
 */
PNP_EXTERN void pn_raw_connection_set_context(pn_raw_connection_t *connection, void *context);

/**
 * Get the attachments that are associated with a raw connection.
 */
PNP_EXTERN pn_record_t *pn_raw_connection_attachments(pn_raw_connection_t *connection);

/**
 * Return the raw connection associated with an event.
 *
 * @return NULL if the event is not associated with a raw connection.
 */
PNP_EXTERN pn_raw_connection_t *pn_event_raw_connection(pn_event_t *event);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* raw_connection.h */
