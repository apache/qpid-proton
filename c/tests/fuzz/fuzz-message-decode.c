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

#include <stdint.h>
#include <stdlib.h>

#include "proton/message.h"

#include "libFuzzingEngine.h"

/*
 * pn_message_decode() (c/src/core/message.c) only scans the wire-level
 * section framing (header / properties / delivery-annotations /
 * message-annotations / application-properties / body) and stashes each
 * section's *raw*, undecoded bytes on the pn_message_t. It never calls into
 * the generic AMQP codec (c/src/core/codec.c, decoder.c) on any of those
 * byte ranges. That only happens lazily -- the first time something calls
 * one of the pn_message_{instructions,annotations,properties,body}()
 * accessors, which route through pni_switch_to_data() (c/src/core/util.h)
 * -> pn_data_decode() -> the real recursive decoder in decoder.c.
 *
 * Previously this harness only ever called pn_message_decode() and threw
 * the result away, so none of those accessors were ever invoked and the
 * fuzzer's input bytes never actually reached codec.c/decoder.c/encoder.c.
 *
 * Force that decode here so the fuzzer's own input bytes actually drive the
 * codec, then force a full read-side traversal of each resulting pn_data_t
 * via pn_data_format() -- which recursively walks the decoded tree with
 * pn_data_next()/pn_data_enter()/pn_data_exit() and the type-specific
 * pn_data_get_*() accessors, so nested lists/maps/arrays/described values
 * are actually visited and not just the outermost node. Finally re-encode
 * the message (pn_message_encode2()) to drive the corresponding encoder.c
 * paths on the same decoded content -- this is the harness's own
 * long-standing "FUTURE" comment, now implemented.
 */
static void pni_force_data_traversal(pn_data_t *data) {
  if (!data) return;
  pn_data_rewind(data);
  char buf[4096];
  size_t size = sizeof(buf);
  pn_data_format(data, buf, &size);
  pn_data_rewind(data);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size < 1) {
    // pn_message_decode would die on assert
    return 0;
  }
  pn_message_t *msg = pn_message();
  int ret = pn_message_decode(msg, (const char *)Data, Size);
  if (ret == 0) {
    // Force real decode + traversal of each lazily-decoded section.
    pni_force_data_traversal(pn_message_instructions(msg));
    pni_force_data_traversal(pn_message_annotations(msg));
    pni_force_data_traversal(pn_message_properties(msg));
    pni_force_data_traversal(pn_message_body(msg));

    // Round-trip the decoded message back to bytes: exercises encoder.c on
    // the same fuzzer-controlled content.
    pn_rwbytes_t buf = {0, NULL};
    pn_message_encode2(msg, &buf);
    free(buf.start);
  }
  if (msg != NULL) {
    pn_message_free(msg);
  }
  return 0;
}
