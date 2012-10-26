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

#ifndef PROTON_SASL_INTERNAL_H
#define PROTON_SASL_INTERNAL_H 1

#include <proton/sasl.h>

/** Decode input data bytes into SASL frames, and process them.
 *
 * This function is called by the driver layer to pass data received
 * from the remote peer into the SASL layer.
 *
 * @param[in] sasl the SASL layer.
 * @param[in] bytes buffer of frames to process
 * @param[in] available number of octets of data in 'bytes'
 * @return the number of bytes consumed, or error code if < 0
 */
ssize_t pn_sasl_input(pn_sasl_t *sasl, const char *bytes, size_t available);

/** Gather output frames from the layer.
 *
 * This function is used by the driver to poll the SASL layer for data
 * that will be sent to the remote peer.
 *
 * @param[in] sasl The SASL layer.
 * @param[out] bytes to be filled with encoded frames.
 * @param[in] size space available in bytes array.
 * @return the number of octets written to bytes, or error code if < 0
 */
ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size);

void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace);

/** Destructor for the given SASL layer.
 *
 * @param[in] sasl the SASL object to free. No longer valid on
 *                 return.
 */
void pn_sasl_free(pn_sasl_t *sasl);

#endif /* sasl-internal.h */
