#ifndef CORE_MESSAGE_INTERNAL_H
#define CORE_MESSAGE_INTERNAL_H

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
 */

#include <proton/message.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL */

/** Construct a message with extra storage */
PN_EXTERN pn_message_t * pni_message_with_extra(size_t extra);

/** Pointer to extra space allocated by pn_message_with_extra(). */
PN_EXTERN void* pni_message_get_extra(pn_message_t *msg);

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif // CORE_MESSAGE_INTERNAL_H
