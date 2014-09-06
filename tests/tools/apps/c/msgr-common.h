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
 *
 */

#include "pncompat/misc_defs.h"

#if defined(USE_INTTYPES)
#ifdef __cplusplus
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#endif

#ifdef _MSC_VER
#if !defined(PRIu64)
#define PRIu64 "I64u"
#endif
#if !defined(SCNu64)
#define SCNu64 "I64u"
#endif
#endif

/* If still not defined, best guess */
#if !defined(SCNu64)
#define SCNu64 "ul"
#endif
#if !defined(PRIu64)
#define PRIu64 "ul"
#endif


#include "proton/types.h"
#include "proton/message.h"

void msgr_die(const char *file, int line, const char *message);
char *msgr_strdup( const char *src );
pn_timestamp_t msgr_now(void);
void parse_password( const char *, char ** );

#define check_messenger(m)  \
  { check(pn_messenger_errno(m) == 0, pn_error_text(pn_messenger_error(m))) }

#define check( expression, message )  \
  { if (!(expression)) msgr_die(__FILE__,__LINE__, message); }


// manage an ordered list of addresses

typedef struct {
  const char **addresses;
  int size;     // room in 'addresses'
  int count;    // # entries
} Addresses_t;

#define NEXT_ADDRESS(a, i) (((i) + 1) % (a).count)
void addresses_init( Addresses_t *a );
void addresses_free( Addresses_t *a );
void addresses_add( Addresses_t *a, const char *addr );
void addresses_merge( Addresses_t *a, const char *list );

// Statistics handling

typedef struct {
  pn_timestamp_t start;
  uint64_t latency_samples;
  double latency_total;
  double latency_min;
  double latency_max;
} Statistics_t;

void statistics_start( Statistics_t *s );
void statistics_msg_received( Statistics_t *s, pn_message_t *message );
void statistics_report( Statistics_t *s, uint64_t sent, uint64_t received );

void enable_logging(void);
void LOG( const char *fmt, ... );

