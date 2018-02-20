#ifndef CORE_CONNECTION_DRIVER_INTERNAL_H
#define CORE_CONNECTION_DRIVER_INTERNAL_H

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

#include <proton/import_export.h>
#include <proton/connection_driver.h>

/**
 * @return pointer to the pn_connection_driver_t* field in a pn_connection_t.
 * Only for use by IO integration code (e.g. pn_proactor_t implementations use this pointer)
 *
 * Return type is pointer to a pointer so that the caller can (if desired) use
 * atomic operations when loading and storing the value.
 */
PN_EXTERN pn_connection_driver_t **pn_connection_driver_ptr(pn_connection_t *connection);

#endif // CORE_CONNECTION_DRIVER_INTERNAL_H
