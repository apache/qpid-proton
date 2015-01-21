#ifndef PROTON_HANDLERS_H
#define PROTON_HANDLERS_H 1

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
#include <proton/type_compat.h>
#include <proton/reactor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Reactor API for proton.
 *
 * @defgroup handlers Handlers
 * @ingroup handlers
 * @{
 */

typedef pn_handler_t pn_handshaker_t;
typedef pn_handler_t pn_iohandler_t;
typedef pn_handler_t pn_flowcontroller_t;

PN_EXTERN pn_handshaker_t *pn_handshaker(void);
PN_EXTERN pn_iohandler_t *pn_iohandler(void);
PN_EXTERN pn_flowcontroller_t *pn_flowcontroller(int window);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* handlers.h */
