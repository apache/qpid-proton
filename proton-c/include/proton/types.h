#ifndef PROTON_TYPES_H
#define PROTON_TYPES_H 1

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
#include <sys/types.h>
#ifndef __cplusplus
#include <stdint.h>
#else
#include <proton/type_compat.h>
#endif

/**
 * @file
 *
 * @defgroup types Types
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup primitives Primitive Types
 * @{
 */

typedef int32_t  pn_sequence_t;
typedef uint32_t pn_millis_t;
typedef uint32_t pn_seconds_t;
typedef int64_t  pn_timestamp_t;
typedef uint32_t pn_char_t;
typedef uint32_t pn_decimal32_t;
typedef uint64_t pn_decimal64_t;
typedef struct {
  char bytes[16];
} pn_decimal128_t;
typedef struct {
  char bytes[16];
} pn_uuid_t;

typedef struct {
  size_t size;
  char *start;
} pn_bytes_t;

PN_EXTERN pn_bytes_t pn_bytes(size_t size, char *start);
PN_EXTERN pn_bytes_t pn_bytes_dup(size_t size, const char *start);

/** @}
 */

/**
 * @defgroup abstract Abstract Types
 * @{
 */

/**
 * Encodes the state of an endpoint.
 * @ingroup connection
 */
typedef int pn_state_t;

/**
 * Encapsulates the endpoint state associated with an AMQP Connection.
 * @ingroup connection
 */
typedef struct pn_connection_t pn_connection_t;

/**
 * Encapsulates the endpoint state associated with an AMQP Session.
 * @ingroup session
 */
typedef struct pn_session_t pn_session_t;

/**
 * Encapsulates the endpoint state associated with an AMQP Link.
 * @ingroup link
 */
typedef struct pn_link_t pn_link_t;

/**
 * Encapsulates the endpoint state associated with an AMQP Delivery.
 * @ingroup delivery
 */
typedef struct pn_delivery_t pn_delivery_t;

/**
 * An event collector.
 * @ingroup event
 */
typedef struct pn_collector_t pn_collector_t;

/**
 * Encapsulates the transport state of all AMQP endpoints associated
 * with a physical network connection.
 * @ingroup transport
 */

typedef struct pn_transport_t pn_transport_t;

/** @}
 */
#ifdef __cplusplus
}
#endif

/** @}
 */

#endif /* types.h */
