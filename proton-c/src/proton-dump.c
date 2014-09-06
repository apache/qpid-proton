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

#include "pncompat/misc_funcs.inc"

#include <stdio.h>
#include <proton/buffer.h>
#include <proton/codec.h>
#include <proton/error.h>
#include <proton/framing.h>
#include "util.h"

void fatal_error(const char *msg, const char *arg, int err)
{
  fprintf(stderr, msg, arg);
  fflush(stderr);
  errno = err;
  perror(" , exiting");
  exit(1);
}

int dump(const char *file)
{
  FILE *in = fopen(file, "r");
  if (!in) fatal_error("proton-dump: dump: opening %s", file, errno);

  pn_buffer_t *buf = pn_buffer(1024);
  pn_data_t *data = pn_data(16);
  bool header = false;

  char bytes[1024];
  size_t n;
  while ((n = fread(bytes, 1, 1024, in))) {
    int err = pn_buffer_append(buf, bytes, n);
    if (err) return err;

    while (true) {
      pn_bytes_t available = pn_buffer_bytes(buf);
      if (!available.size) break;

      if (!header) {
        if (available.size >= 8) {
          pn_buffer_trim(buf, 8, 0);
          available = pn_buffer_bytes(buf);
          header = true;
        } else {
          break;
        }
      }

      pn_frame_t frame;
      size_t consumed = pn_read_frame(&frame, available.start, available.size);
      if (consumed) {
        pn_data_clear(data);
        ssize_t dsize = pn_data_decode(data, frame.payload, frame.size);
        if (dsize < 0) {
          fprintf(stderr, "Error decoding frame: %s\n", pn_code(err));
          pn_fprint_data(stderr, frame.payload, frame.size);
          fprintf(stderr, "\n");
          return err;
        } else {
          pn_data_print(data);
          printf("\n");
        }
        pn_buffer_trim(buf, consumed, 0);
      } else {
        break;
      }
    }
  }

  if (ferror(in)) fatal_error("proton-dump: dump: reading %s", file, errno);
  if (pn_buffer_size(buf) > 0) {
    fprintf(stderr, "Trailing data: ");
    pn_bytes_t b = pn_buffer_bytes(buf);
    pn_fprint_data(stderr, b.start, b.size);
    fprintf(stderr, "\n");
  }

  fclose(in);

  return 0;
}

void usage(char* prog) {
  printf("Usage: %s [FILE1] [FILEn] ...\n", prog);
  printf("Displays the content of an AMQP dump file containing frame data.\n");
  printf("\n  [FILEn]  Dump file to be displayed.\n\n");
}

int main(int argc, char **argv)
{
  if(argc == 1) {
    usage(argv[0]);
    return 0;
  }

  int c;

  while ( (c = getopt(argc, argv, "h")) != -1 ) {
    switch(c) {
    case 'h':
      usage(argv[0]);
      return 0;
      break;

    case '?':
      usage(argv[0]);
      return 1;
    }
  }

  for (int i = 1; i < argc; i++) {
    int err = dump(argv[i]);
    if (err) return err;
  }

  return 0;
}
