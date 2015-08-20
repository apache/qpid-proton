#ifndef PROTON_ERROR_H
#define PROTON_ERROR_H 1

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
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A pn_error_t has an int error `code` and some string `text` to describe the error */ 
typedef struct pn_error_t pn_error_t;

#define PN_OK (0)
#define PN_EOS (-1)
#define PN_ERR (-2)
#define PN_OVERFLOW (-3)
#define PN_UNDERFLOW (-4)
#define PN_STATE_ERR (-5)
#define PN_ARG_ERR (-6)
#define PN_TIMEOUT (-7)
#define PN_INTR (-8)
#define PN_INPROGRESS (-9)
#define PN_OUT_OF_MEMORY (-10)

/** @return name of the error code. Returned pointer is to a static constant, do not delete.*/ 
PN_EXTERN const char *pn_code(int code);

PN_EXTERN pn_error_t *pn_error(void);
PN_EXTERN void pn_error_free(pn_error_t *error);

/** Reset the error to a "no error" state with code == 0. */
PN_EXTERN void pn_error_clear(pn_error_t *error);

/** Set the error code and text. Makes a copy of text. */
PN_EXTERN int pn_error_set(pn_error_t *error, int code, const char *text);

/** Set the code and set the text using a printf-style formatted string. */
PN_EXTERN int pn_error_vformat(pn_error_t *error, int code, const char *fmt, va_list ap);

/** Set the code and set the text using a printf-style formatted string. */
PN_EXTERN int pn_error_format(pn_error_t *error, int code, const char *fmt, ...);

/** @return the error code. */
PN_EXTERN int pn_error_code(pn_error_t *error);

/** @return the error text.  Returned pointer is owned by the pn_error_t. */
PN_EXTERN const char *pn_error_text(pn_error_t *error);

PN_EXTERN int pn_error_copy(pn_error_t *error, pn_error_t *src);

#define PN_RETURN_IF_ERROR(x) \
do {\
int r = (x);\
if (r < 0) return r; \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* error.h */
