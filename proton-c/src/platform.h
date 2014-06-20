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

#ifdef __cplusplus
extern "C" {
#endif

/** Get the current time in pn_timestamp_t format.
 *
 * Returns current time in milliseconds since Unix Epoch,
 * as defined by AMQP 1.0
 *
 * @return current time
 * @internal
 */
pn_timestamp_t pn_i_now(void);

/** Generate a UUID in string format.
 *
 * Returns a newly generated UUID in the standard 36 char format.
 * The returned char* array is zero terminated.
 * (eg. d797830d-0f5b-49d4-a83f-adaa78425125)
 *
 * @return newly generated stringised UUID
 * @internal
 */
char* pn_i_genuuid(void);

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

/** Provide C99 atoll functinality.
 *
 * @param[in] num the string representation of the number.
 * @return the integer value.
 *
 * @internal
 */
int64_t pn_i_atoll(const char* num);

#ifdef _MSC_VER
/** Windows snprintf and vsnprintf substitutes.
 *
 * Provide the expected C99 behavior for these functions.
 */
#include <stdio.h>
#define snprintf pn_i_snprintf
#define vsnprintf pn_i_vsnprintf
int pn_i_snprintf(char *buf, size_t count, const char *fmt, ...);
int pn_i_vsnprintf(char *buf, size_t count, const char *fmt, va_list ap);
#endif

#if defined _MSC_VER || defined _OPENVMS
#if !defined(va_copy)
#define va_copy(d,s) ((d) = (s))
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* platform.h */
