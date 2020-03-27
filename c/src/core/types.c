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

#include <proton/types.h>

const pn_bytes_t pn_bytes_null = { 0, NULL };
const pn_rwbytes_t pn_rwbytes_null = { 0, NULL };

pn_bytes_t pn_bytes(size_t size, const char *start)
{
  pn_bytes_t bytes = {size, start};
  return bytes;
}

pn_rwbytes_t pn_rwbytes(size_t size, char *start)
{
  pn_rwbytes_t bytes = {size, start};
  return bytes;
}
