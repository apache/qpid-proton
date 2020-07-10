#ifndef PROTON_PLATFORM_H
#define PROTON_PLATFORM_H 1

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

#include "proton/types.h"
#include "proton/error.h"

#if __cplusplus
extern "C" {
#endif

/** Get the current PID
 *
 * @return process id
 * @internal
 */
int pn_i_getpid(void);


/** Get the current time in pn_timestamp_t format.
 *
 * Returns current time in milliseconds since Unix Epoch,
 * as defined by AMQP 1.0
 *
 * @return current time
 * @internal
 */
pn_timestamp_t pn_i_now(void);

/** Generate system error message.
 *
 * Populate the proton error structure based on the last system error
 * code.
 *
 * @param[in] error the proton error structure
 * @param[in] msg the descriptive context message
 * @return error->code
 *
 * @internal
 */
int pn_i_error_from_errno(pn_error_t *error, const char *msg);

#ifdef _MSC_VER

#if !defined(S_ISDIR)
# define S_ISDIR(X) ((X) & _S_IFDIR)
#endif

#endif

#if defined _MSC_VER || defined _OPENVMS
#if !defined(va_copy)
#define va_copy(d,s) ((d) = (s))
#endif
#endif

#if __cplusplus
}
#endif

#endif /* platform.h */
