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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proton/engine.h>

// never remove 'assert()'
#undef NDEBUG
#include <assert.h>

// push data from one transport to another
static int xfer(pn_transport_t *src, pn_transport_t *dest)
{
    ssize_t out = pn_transport_pending(src);
    if (out > 0) {
        ssize_t in = pn_transport_capacity(dest);
        if (in > 0) {
            size_t count = (size_t)((out < in) ? out : in);
            pn_transport_push(dest,
                              pn_transport_head(src),
                              count);
            pn_transport_pop(src, count);
            return (int)count;
        }
    }
    return 0;
}

// transfer all available data between two transports
static int pump(pn_transport_t *t1, pn_transport_t *t2)
{
    int total = 0;
    int work;
    do {
        work = xfer(t1, t2) + xfer(t2, t1);
        total += work;
    } while (work);
    return total;
}

// handle state changes of the endpoints
static void process_endpoints(pn_connection_t *conn)
{
    pn_session_t *ssn = pn_session_head(conn, PN_LOCAL_UNINIT);
    while (ssn) {
        //fprintf(stderr, "Opening session %p\n", (void*)ssn);
        pn_session_open(ssn);
        ssn = pn_session_next(ssn, PN_LOCAL_UNINIT);
    }

    pn_link_t *link = pn_link_head(conn, PN_LOCAL_UNINIT);
    while (link) {
        //fprintf(stderr, "Opening link %p\n", (void*)link);
        pn_link_open(link);
        link = pn_link_next(link, PN_LOCAL_UNINIT);
    }

    link = pn_link_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
    while (link) {
        //fprintf(stderr, "Closing link %p\n", (void*)link);
        pn_link_close(link);
        link = pn_link_next(link, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
    }

    ssn = pn_session_head(conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
    while (ssn) {
        //fprintf(stderr, "Closing session %p\n", (void*)ssn);
        pn_session_close(ssn);
        ssn = pn_session_next(ssn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
    }
}

// bring up a session and a link between the two connections
static void test_setup(pn_connection_t *c1, pn_transport_t *t1,
                       pn_connection_t *c2, pn_transport_t *t2)
{
    pn_connection_open(c1);
    pn_connection_open(c2);

    pn_session_t *s1 = pn_session(c1);
    pn_session_open(s1);

    pn_link_t *tx = pn_sender(s1, "sender");
    pn_link_open(tx);

    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }

    // session and link should be up, c2 should have a receiver link:

    assert(pn_session_state( s1 ) == (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(pn_link_state( tx ) == (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));

    pn_link_t *rx = pn_link_head(c2, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(rx && pn_link_is_receiver(rx));
}

// test that free'ing the connection should free all contained
// resources (session, links, deliveries)
int test_free_connection(int argc, char **argv)
{
    fprintf(stdout, "test_free_connection\n");
    pn_connection_t *c1 = pn_connection();
    pn_transport_t  *t1 = pn_transport();
    pn_transport_bind(t1, c1);

    pn_connection_t *c2 = pn_connection();
    pn_transport_t  *t2 = pn_transport();
    pn_transport_bind(t2, c2);

    //pn_transport_trace(t1, PN_TRACE_FRM);
    test_setup(c1, t1,
               c2, t2);

    pn_link_t *tx = pn_link_head(c1, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(tx);
    pn_link_t *rx = pn_link_head(c2, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(rx);

    // transfer some data across the link:
    pn_link_flow(rx, 10);
    pn_delivery_t *d1 = pn_delivery(tx, pn_dtag("tag-1", 6));
    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }
    assert(pn_delivery_writable(d1));
    pn_link_send(tx, "ABC", 4);
    pn_link_advance(tx);

    // now free the connection, but keep processing the transport
    process_endpoints(c1);
    pn_connection_free(c1);
    while (pump(t1, t2)) {
        process_endpoints(c2);
    }

    // delivery should have transfered:
    assert(pn_link_current(rx) &&
           pn_delivery_readable(pn_link_current(rx)));

    pn_transport_unbind(t1);
    pn_transport_free(t1);

    pn_connection_free(c2);
    pn_transport_unbind(t2);
    pn_transport_free(t2);

    return 0;
}

int test_free_session(int argc, char **argv)
{
    fprintf(stdout, "test_free_session\n");
    pn_connection_t *c1 = pn_connection();
    pn_transport_t  *t1 = pn_transport();
    pn_transport_bind(t1, c1);

    pn_connection_t *c2 = pn_connection();
    pn_transport_t  *t2 = pn_transport();
    pn_transport_bind(t2, c2);

    //pn_transport_trace(t1, PN_TRACE_FRM);
    test_setup(c1, t1,
               c2, t2);

    pn_session_t *ssn = pn_session_head(c1, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(ssn);
    pn_link_t *tx = pn_link_head(c1, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(tx);
    pn_link_t *rx = pn_link_head(c2, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(rx);

    // prepare for transfer: request some credit
    pn_link_flow(rx, 10);
    pn_delivery_t *d1 = pn_delivery(tx, pn_dtag("tag-1", 6));
    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }
    assert(pn_delivery_writable(d1));

    // send some data, but also close the session:
    pn_link_send(tx, "ABC", 4);
    pn_link_advance(tx);

    pn_session_close(ssn);
    pn_session_free(ssn);

    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }

    // delivery should have transfered:
    assert(pn_link_current(rx));
    assert(pn_delivery_readable(pn_link_current(rx)));

    // c2's session should see the close:
    pn_session_t *ssn2 = pn_session_head(c2, 0);
    assert(ssn2 && pn_session_state(ssn2) == (PN_LOCAL_CLOSED | PN_REMOTE_CLOSED));

    pn_transport_unbind(t1);
    pn_transport_free(t1);
    pn_connection_free(c1);

    pn_transport_unbind(t2);
    pn_transport_free(t2);
    pn_connection_free(c2);

    return 0;
}

int test_free_link(int argc, char **argv)
{
    fprintf(stdout, "test_free_link\n");
    pn_connection_t *c1 = pn_connection();
    pn_transport_t  *t1 = pn_transport();
    pn_transport_bind(t1, c1);

    pn_connection_t *c2 = pn_connection();
    pn_transport_t  *t2 = pn_transport();
    pn_transport_bind(t2, c2);

    //pn_transport_trace(t1, PN_TRACE_FRM);
    test_setup(c1, t1,
               c2, t2);

    pn_link_t *tx = pn_link_head(c1, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(tx);
    pn_link_t *rx = pn_link_head(c2, (PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE));
    assert(rx);

    // prepare for transfer: request some credit
    pn_link_flow(rx, 10);
    pn_delivery_t *d1 = pn_delivery(tx, pn_dtag("tag-1", 6));
    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }
    assert(pn_delivery_writable(d1));

    // send some data, then close and destroy the link:
    pn_link_send(tx, "ABC", 4);
    pn_link_advance(tx);

    pn_link_close(tx);
    pn_link_free(tx);

    while (pump(t1, t2)) {
        process_endpoints(c1);
        process_endpoints(c2);
    }

    // the data transfer will complete and the link close
    // should have been sent to the peer
    assert(pn_link_current(rx));
    assert(pn_link_state(rx) == (PN_LOCAL_CLOSED | PN_REMOTE_CLOSED));

    pn_transport_unbind(t1);
    pn_transport_free(t1);
    pn_connection_free(c1);

    pn_transport_unbind(t2);
    pn_transport_free(t2);
    pn_connection_free(c2);

    return 0;
}


typedef int (*test_ptr_t)(int argc, char **argv);

test_ptr_t tests[] = {test_free_connection,
                      test_free_session,
                      test_free_link,
                      NULL};

int main(int argc, char **argv)
{
    test_ptr_t *test = tests;
    while (*test) {
        int rc = (*test++)(argc, argv);
        if (rc)
            return rc;
    }
    return 0;
}
