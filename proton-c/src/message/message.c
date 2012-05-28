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

#include <proton/message.h>
#include <proton/codec.h>
#include "protocol.h"

ssize_t pn_message_data(char *dst, size_t available, const char *src, size_t size)
{
  pn_bytes_t bytes = pn_bytes(available, dst);
  pn_atom_t buf[16];
  pn_atoms_t atoms = {16, buf};

  int err = pn_fill_atoms(&atoms, "DLz", 0x75, size, src);
  if (err) return err;
  err = pn_encode_atoms(&bytes, &atoms);
  if (err) return err;
  return bytes.size;
}
