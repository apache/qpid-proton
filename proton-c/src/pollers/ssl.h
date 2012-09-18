#ifndef PROTON_SRC_DRIVERS_SSL_H
#define PROTON_SRC_DRIVERS_SSL_H 1
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

/** @file
 * Internal API for SSL/TLS support in the Driver Layer.
 *
 * Generic API to abstract the implementation of SSL/TLS from the Driver codebase.
 *
 */


/** Configure SSL/TLS on a connector that has just been accepted on the given listener.
 *
 * @param[in,out] l the listener that has accepted the connnector c.
 * @param[in,out] c the connector that will be configured for SSL/TLS (client mode).
 * @return 0 on success, else an error code if SSL/TLS cannot be configured.
 */
int pn_ssl_client_init( pn_ssl_t *ssl);

/** Start the SSL/TLS shutdown handshake.
 *
 * The SSL/TLS shutdown involves a protocol handshake.  This call will initiate the
 * shutdown process, which may not complete on return from this function.  Once the
 * handshake is completed, the connector will be closed and pn_connector_closed() will
 * return TRUE.
 *
 * @param[in,out] c the connector to shutdown.
 */
void pn_ssl_shutdown( pn_ssl_t *ssl);

/** Release any SSL/TLS related resources used by the listener.
 *
 * @param[in,out] l the listener to clean up.
 */
void pn_ssl_free( pn_ssl_t *ssl);

/** Check if the SSL/TLS layer has data ready for reading or writing
 *
 * @param[in] d the driver
 * @return 0 if no data ready, else !0
 */
int pn_driver_ssl_data_ready( pn_driver_t *d );

#endif /* ssl.h */
