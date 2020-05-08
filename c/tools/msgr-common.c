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

#include "msgr-common.h"
#include <pncompat/misc_funcs.inc>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void msgr_die(const char *file, int line, const char *message)
{
  fprintf(stderr, "%s:%i: %s\n", file, line, message);
  exit(1);
}

//sigh - would be nice if proton exported pn_strdup()
char *msgr_strdup( const char *src )
{
  char *r = NULL;
  if (src) {
    r = (char *) malloc(sizeof(char) * (strlen(src) + 1));
    if (r) strcpy(r,src);
  }
  return r;
}


pn_timestamp_t msgr_now()
{
  // from "pncompat/misc_funcs.inc"
  return time_now();
}

void addresses_init( Addresses_t *a )
{
  a->size = 10; // whatever
  a->count = 0;
  a->addresses = (const char **) calloc( a->size, sizeof(const char *));
  check(a->addresses, "malloc failure");
}

void addresses_free( Addresses_t *a )
{
  if (a->addresses) {
    int i;
    for (i = 0; i < a->count; i++)
      if (a->addresses[i]) free( (void *)a->addresses[i] );
    free( (void *) a->addresses );
    a->addresses = NULL;
  }
}

void addresses_add( Addresses_t *a, const char *addr )
{
  if (a->count == a->size) {
    a->size += 10;
    a->addresses = (const char **) realloc( a->addresses,
                                            a->size * sizeof(const char *) );
    check( a->addresses, "malloc failure" );
    int i;
    for (i = a->count; i < a->size; i++)
      a->addresses[i] = NULL;
  }
  a->addresses[a->count] = msgr_strdup(addr);
  check( a->addresses[a->count], "malloc failure" );
  a->count++;
}

// merge a comma-separated list of addresses
void addresses_merge( Addresses_t *a, const char *list )
{
  char *const l = msgr_strdup(list);
  check( l, "malloc failure" );
  char *addr = l;
  while (addr && *addr) {
    char *comma = strchr( addr, ',' );
    if (comma) {
      *comma++ = '\0';
    }
    addresses_add( a, addr );
    addr = comma;
  }
  free(l);
}


void statistics_start( Statistics_t *s )
{
  s->latency_samples = 0;
  s->latency_total = s->latency_min = s->latency_max = 0.0;
  s->start = msgr_now();
}

void statistics_msg_received( Statistics_t *s, pn_message_t *message )
{
  pn_timestamp_t ts = pn_message_get_creation_time( message );
  if (ts) {
    double l = (double)(msgr_now() - ts);
    if (l > 0) {
      s->latency_total += l;
      if (++s->latency_samples == 1) {
        s->latency_min = s->latency_max = l;
      } else {
        if (s->latency_min > l)
          s->latency_min = l;
        if (s->latency_max < l)
          s->latency_max = l;
      }
    }
  }
}

void statistics_report( Statistics_t *s, uint64_t sent, uint64_t received )
{
  pn_timestamp_t end = msgr_now() - s->start;
  double secs = end/(double)1000.0;

  fprintf(stdout, "Messages sent: %" PRIu64 " recv: %" PRIu64 "\n", sent, received );
  fprintf(stdout, "Total time: %f sec\n", secs );
  fprintf(stdout, "Throughput: %f msgs/sec\n",  (secs != 0.0) ? (double)sent/secs : 0);
  fprintf(stdout, "Latency (sec): %f min %f max %f avg\n",
          s->latency_min/1000.0, s->latency_max/1000.0,
          (s->latency_samples) ? (s->latency_total/s->latency_samples)/1000.0 : 0);
}

void parse_password( const char *input, char **password )
{
    if (strncmp( input, "pass:", 5 ) == 0) {
        // password provided on command line (not secure, shows up in 'ps')
        *password = msgr_strdup( input + 5 );
    } else {    // input assumed to be file containing password
        FILE *f = fopen( input, "r" );
        check( f, "Cannot open password file\n" );
        *password = (char *)malloc(256);    // 256 should be enough for anybody!
        check( *password, "malloc failure" );
        int rc = fscanf( f, "%255s", *password );
        check( rc == 1, "Cannot read password from file\n" );
        fclose(f);
    }
}

static int dolog = 0;
void enable_logging()
{
    dolog = 1;
}

void LOG( const char *fmt, ... )
{
    if (dolog) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf( stdout, fmt, ap );
        va_end(ap);
    }
}
