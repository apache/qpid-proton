#ifndef PNCOMAPT_MISC_DEFS_H
#define PNCOMAPT_MISC_DEFS_H

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

#if defined(qpid_proton_EXPORTS)
#error This include file is not for use in the main proton library
#endif

/*
 * Platform neutral definitions. Only intended for use by Proton
 * examples and test/debug programs.
 *
 * This file and any related support files may change or be removed
 * at any time.
 */

// getopt()

#include <proton/types.h>

#if !defined(_WIN32) || defined (__CYGWIN__)
#include <getopt.h>
#else
#include "internal/getopt.h"
#endif

pn_timestamp_t time_now(void);

#endif /* PNCOMPAT_MISC_DEFS_H */
