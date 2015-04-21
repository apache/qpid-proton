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

#include "sasl-internal.h"

#include "engine/engine-internal.h"
#include "transport/autodetect.h"

#include <assert.h>

static inline pn_transport_t *get_transport_internal(pn_sasl_t *sasl)
{
    // The external pn_sasl_t is really a pointer to the internal pni_transport_t
    return ((pn_transport_t *)sasl);
}

void pn_sasl_allow_skip(pn_sasl_t *sasl0, bool allow)
{
    if (!sasl0) return;
    pn_transport_t *transport = get_transport_internal(sasl0);
    pn_transport_require_auth(transport, !allow);
}

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available);
static ssize_t pn_input_read_sasl(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available);
static ssize_t pn_output_write_sasl_header(pn_transport_t* transport, unsigned int layer, char* bytes, size_t size);
static ssize_t pn_output_write_sasl(pn_transport_t *transport, unsigned int layer, char *bytes, size_t available);

const pn_io_layer_t sasl_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl_header,
    NULL,
    NULL
};

const pn_io_layer_t sasl_write_header_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl_header,
    NULL,
    NULL
};

const pn_io_layer_t sasl_read_header_layer = {
    pn_input_read_sasl_header,
    pn_output_write_sasl,
    NULL,
    NULL
};

const pn_io_layer_t sasl_layer = {
    pn_input_read_sasl,
    pn_output_write_sasl,
    NULL,
    NULL
};

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")
#define SASL_HEADER_LEN 8

static ssize_t pn_input_read_sasl_header(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  bool eos = pn_transport_capacity(transport)==PN_EOS;
  pni_protocol_type_t protocol = pni_sniff_header(bytes, available);
  switch (protocol) {
  case PNI_PROTOCOL_AMQP_SASL:
    if (transport->io_layers[layer] == &sasl_read_header_layer) {
        transport->io_layers[layer] = &sasl_layer;
    } else {
        transport->io_layers[layer] = &sasl_write_header_layer;
    }
    if (transport->trace & PN_TRACE_FRM)
        pn_transport_logf(transport, "  <- %s", "SASL");
    return SASL_HEADER_LEN;
  case PNI_PROTOCOL_INSUFFICIENT:
    if (!eos) return 0;
    /* Fallthru */
  default:
    break;
  }
  transport->close_sent = true;
  char quoted[1024];
  pn_quote_data(quoted, 1024, bytes, available);
  pn_do_error(transport, "amqp:connection:framing-error",
              "%s header mismatch: %s ['%s']%s", "SASL", pni_protocol_name(protocol), quoted,
              !eos ? "" : " (connection aborted)");
  pn_set_error_layer(transport);
  return PN_EOS;
}

static ssize_t pn_input_read_sasl(pn_transport_t* transport, unsigned int layer, const char* bytes, size_t available)
{
  bool eos = pn_transport_capacity(transport)==PN_EOS;
  if (eos) {
    transport->close_sent = true;
    pn_do_error(transport, "amqp:connection:framing-error", "connection aborted");
    pn_set_error_layer(transport);
    return PN_EOS;
  }

  if (!transport->sasl_input_bypass) {
    ssize_t n = pn_sasl_input(transport, bytes, available);
    if (n != PN_EOS) return n;

    transport->sasl_input_bypass = true;
    if (transport->sasl_output_bypass)
        transport->io_layers[layer] = &pni_passthru_layer;
  }
  return pni_passthru_layer.process_input(transport, layer, bytes, available );
}

static ssize_t pn_output_write_sasl_header(pn_transport_t *transport, unsigned int layer, char *bytes, size_t size)
{
  if (transport->trace & PN_TRACE_FRM)
    pn_transport_logf(transport, "  -> %s", "SASL");
  assert(size >= SASL_HEADER_LEN);
  memmove(bytes, SASL_HEADER, SASL_HEADER_LEN);
  if (transport->io_layers[layer]==&sasl_write_header_layer) {
      transport->io_layers[layer] = &sasl_layer;
  } else {
      transport->io_layers[layer] = &sasl_read_header_layer;
  }
  return SASL_HEADER_LEN;
}

static ssize_t pn_output_write_sasl(pn_transport_t* transport, unsigned int layer, char* bytes, size_t available)
{
  if (!transport->sasl_output_bypass) {
    // this accounts for when pn_do_error is invoked, e.g. by idle timeout
    ssize_t n;
    if (transport->close_sent) {
        n = PN_EOS;
    } else {
        n = pn_sasl_output(transport, bytes, available);
    }
    if (n != PN_EOS) return n;

    transport->sasl_output_bypass = true;
    if (transport->sasl_input_bypass)
        transport->io_layers[layer] = &pni_passthru_layer;
  }
  return pni_passthru_layer.process_output(transport, layer, bytes, available );
}

