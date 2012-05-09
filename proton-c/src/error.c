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

#include <proton/errors.h>

const char *pn_error(int err)
{
  switch (err)
  {
  case 0: return "<ok>";
  case PN_EOS: return "PN_EOS";
  case PN_ERR: return "PN_ERR";
  case PN_OVERFLOW: return "PN_OVERFLOW";
  case PN_UNDERFLOW: return "PN_UNDERFLOW";
  case PN_STATE_ERR: return "PN_STATE_ERR";
  case PN_ARG_ERR: return "PN_ARG_ERR";
  default: return "<unknown>";
  }
}
