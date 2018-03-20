#ifndef _PROTON_DISPATCH_ACTIONS_H
#define _PROTON_DISPATCH_ACTIONS_H 1

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

#include "dispatcher.h"

#define AMQP_FRAME_TYPE (0)
#define SASL_FRAME_TYPE (1)


/* AMQP actions */
int pn_do_open(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_begin(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_attach(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_transfer(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_flow(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_disposition(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_detach(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_end(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_close(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);

/* SASL actions */
int pn_do_init(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_mechanisms(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_challenge(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_response(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);
int pn_do_outcome(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);

#endif // _PROTON_DISPATCH_ACTIONS_H
