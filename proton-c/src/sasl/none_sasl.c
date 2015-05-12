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

static const char ANONYMOUS[] = "ANONYMOUS";
static const char PLAIN[] = "PLAIN";

bool pni_init_server(pn_transport_t* transport)
{
  return true;
}

bool pni_init_client(pn_transport_t* transport)
{
  return true;
}

void pni_sasl_impl_free(pn_transport_t *transport)
{
  free(transport->sasl->impl_context);
}

// Client handles ANONYMOUS or PLAIN mechanisms if offered
bool pni_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
  // Check whether offered ANONYMOUS or PLAIN
  // Look for "PLAIN" in mechs
  const char *found = strstr(mechs, PLAIN);
  // Make sure that string is separated and terminated, allowed
  // and we have a username and password
  if (found && (found==mechs || found[-1]==' ') && (found[5]==0 || found[5]==' ') &&
      pni_included_mech(transport->sasl->included_mechanisms, pn_bytes(5, found)) &&
      transport->sasl->username && transport->sasl->password) {
    transport->sasl->selected_mechanism = pn_strdup(PLAIN);
    size_t usize = strlen(transport->sasl->username);
    size_t psize = strlen(transport->sasl->password);
    size_t size = usize + psize + 2;
    char *iresp = (char *) malloc(size);
    if (!iresp) return false;

    transport->sasl->impl_context = iresp;

    iresp[0] = 0;
    memmove(iresp + 1, transport->sasl->username, usize);
    iresp[usize + 1] = 0;
    memmove(iresp + usize + 2, transport->sasl->password, psize);
    transport->sasl->bytes_out.start = iresp;
    transport->sasl->bytes_out.size =  size;

    // Zero out password and dealloc
    free(memset(transport->sasl->password, 0, psize));
    transport->sasl->password = NULL;

    return true;
  }

  // Look for "ANONYMOUS" in mechs
  found = strstr(mechs, ANONYMOUS);
  // Make sure that string is separated and terminated and allowed
  if (found && (found==mechs || found[-1]==' ') && (found[9]==0 || found[9]==' ') &&
      pni_included_mech(transport->sasl->included_mechanisms, pn_bytes(9, found))) {
    transport->sasl->selected_mechanism = pn_strdup(ANONYMOUS);
    if (transport->sasl->username) {
      size_t size = strlen(transport->sasl->username);
      char *iresp = (char *) malloc(size);
      if (!iresp) return false;

      transport->sasl->impl_context = iresp;

      memmove(iresp, transport->sasl->username, size);
      transport->sasl->bytes_out.start = iresp;
      transport->sasl->bytes_out.size =  size;
    } else {
      static const char anon[] = "anonymous";
      transport->sasl->bytes_out.start = anon;
      transport->sasl->bytes_out.size =  sizeof anon-1;
    }
    return true;
  }
  return false;
}

// Server will offer only ANONYMOUS
int pni_sasl_impl_list_mechs(pn_transport_t *transport, char **mechlist)
{
  *mechlist = pn_strdup("ANONYMOUS");
  return 1;
}

void pni_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
  // Check that mechanism is ANONYMOUS and it is allowed
  if (strcmp(mechanism, "ANONYMOUS")==0 &&
      pni_included_mech(transport->sasl->included_mechanisms, pn_bytes(sizeof(ANONYMOUS)-1, ANONYMOUS))) {
    transport->sasl->username = "anonymous";
    transport->sasl->outcome = PN_SASL_OK;
    pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
  } else {
    transport->sasl->outcome = PN_SASL_AUTH;
    pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
  }
}

/* The default implementation neither sends nor receives challenges or responses */
void pni_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
}

void pni_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
}
