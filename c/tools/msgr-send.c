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

#define PN_USE_DEPRECATED_API 1

#include "msgr-common.h"
#include "proton/message.h"
#include "proton/messenger.h"
#include "proton/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    Addresses_t targets;
    uint64_t msg_count;
    uint32_t msg_size;  // of body
    uint32_t send_batch;
    int   outgoing_window;
    unsigned int report_interval;      // in seconds
    //Addresses_t subscriptions;
    //Addresses_t reply_tos;
    int   get_replies;
    int   timeout;      // in seconds
    int   incoming_window;
    int   recv_count;
    const char *name;
    char *certificate;
    char *privatekey;   // used to sign certificate
    char *password;     // for private key file
    char *ca_db;        // trusted CA database
} Options_t;


static void usage(int rc)
{
    printf("Usage: msgr-send [OPTIONS] \n"
           " -a <addr>[,<addr>]* \tThe target address [amqp[s]://domain[/name]]\n"
           " -c # \tNumber of messages to send before exiting [0=forever]\n"
           " -b # \tSize of message body in bytes [1024]\n"
           " -p # \tSend batches of # messages (wait for replies before sending next batch if -R) [1024]\n"
           " -w # \t# outgoing window size [0]\n"
           " -e # \t# seconds to report statistics, 0 = end of test [0]\n"
           " -R \tWait for a reply to each sent message\n"
           " -t # \tInactivity timeout in seconds, -1 = no timeout [-1]\n"
           " -W # \tIncoming window size [0]\n"
           " -B # \tArgument to Messenger::recv(n) [-1]\n"
           " -N <name> \tSet the container name to <name>\n"
           " -V \tEnable debug logging\n"
           " SSL options:\n"
           " -T <path> \tDatabase of trusted CA certificates for validating peer\n"
           " -C <path> \tFile containing self-identifying certificate\n"
           " -K <path> \tFile containing private key used to sign certificate\n"
           " -P [pass:<password>|path] \tPassword to unlock private key file.\n"
           );
    //printf("-p     \t*TODO* Add N sample properties to each message [3]\n");
    exit(rc);
}

static void parse_options( int argc, char **argv, Options_t *opts )
{
    int c;
    opterr = 0;

    memset( opts, 0, sizeof(*opts) );
    opts->msg_size  = 1024;
    opts->send_batch = 1024;
    opts->timeout = -1;
    opts->recv_count = -1;
    addresses_init(&opts->targets);

    while ((c = getopt(argc, argv,
                       "a:c:b:p:w:e:l:Rt:W:B:VN:T:C:K:P:")) != -1) {
        switch(c) {
        case 'a': addresses_merge( &opts->targets, optarg ); break;
        case 'c':
            if (sscanf( optarg, "%" SCNu64, &opts->msg_count ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'b':
            if (sscanf( optarg, "%u", &opts->msg_size ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'p':
            if (sscanf( optarg, "%u", &opts->send_batch ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'w':
            if (sscanf( optarg, "%d", &opts->outgoing_window ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'e':
            if (sscanf( optarg, "%u", &opts->report_interval ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'R': opts->get_replies = 1; break;
        case 't':
            if (sscanf( optarg, "%d", &opts->timeout ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            if (opts->timeout > 0) opts->timeout *= 1000;
            break;
        case 'W':
            if (sscanf( optarg, "%d", &opts->incoming_window ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'B':
            if (sscanf( optarg, "%d", &opts->recv_count ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'V': enable_logging(); break;
        case 'N': opts->name = optarg; break;
        case 'T': opts->ca_db = optarg; break;
        case 'C': opts->certificate = optarg; break;
        case 'K': opts->privatekey = optarg; break;
        case 'P': parse_password( optarg, &opts->password ); break;

        default:
            usage(1);
        }
    }

    // default target if none specified
    if (opts->targets.count == 0) addresses_add( &opts->targets, "amqp://0.0.0.0" );
}


// return the # of reply messages received
static int process_replies( pn_messenger_t *messenger,
                            pn_message_t *message,
                            Statistics_t *stats,
                            int max_count)
{
    int received = 0;
    LOG("Calling pn_messenger_recv(%d)\n", max_count);
    int rc = pn_messenger_recv( messenger, max_count );
    check((rc == 0 || rc == PN_TIMEOUT), "pn_messenger_recv() failed");
    LOG("Messages on incoming queue: %d\n", pn_messenger_incoming(messenger));
    while (pn_messenger_incoming(messenger)) {
        pn_messenger_get(messenger, message);
        check_messenger(messenger);
        received++;
        // TODO: header decoding?
        statistics_msg_received( stats, message );
        // uint64_t id = pn_message_get_correlation_id( message ).u.as_ulong;
    }
    return received;
}



int main(int argc, char** argv)
{
    Options_t opts;
    Statistics_t stats;
    uint64_t sent = 0;
    uint64_t received = 0;
    int target_index = 0;
    int rc;

    pn_message_t *message = 0;
    pn_message_t *reply_message = 0;
    pn_messenger_t *messenger = 0;

    parse_options( argc, argv, &opts );

    messenger = pn_messenger( opts.name );

    if (opts.certificate) {
        rc = pn_messenger_set_certificate(messenger, opts.certificate);
        check( rc == 0, "Failed to set certificate" );
    }

    if (opts.privatekey) {
        rc = pn_messenger_set_private_key(messenger, opts.privatekey);
        check( rc == 0, "Failed to set private key" );
    }

    if (opts.password) {
        rc = pn_messenger_set_password(messenger, opts.password);
        free(opts.password);
        check( rc == 0, "Failed to set password" );
    }

    if (opts.ca_db) {
        rc = pn_messenger_set_trusted_certificates(messenger, opts.ca_db);
        check( rc == 0, "Failed to set trusted CA database" );
    }

    if (opts.outgoing_window) {
        pn_messenger_set_outgoing_window( messenger, opts.outgoing_window );
    }
    pn_messenger_set_timeout( messenger, opts.timeout );
    pn_messenger_start(messenger);

    message = pn_message();
    check(message, "failed to allocate a message");
    pn_message_set_reply_to(message, "~");
    pn_data_t *body = pn_message_body(message);
    char *data = (char *)calloc(1, opts.msg_size);
    pn_data_put_binary(body, pn_bytes(opts.msg_size, data));
    free(data);
    pn_atom_t id;
    id.type = PN_ULONG;

#if 0
    // TODO: how do we effectively benchmark header processing overhead???
    pn_data_t *props = pn_message_properties(message);
    pn_data_put_map(props);
    pn_data_enter(props);
    //
    //pn_data_put_string(props, pn_bytes(6,  "string"));
    //pn_data_put_string(props, pn_bytes(10, "this is awkward"));
    //
    //pn_data_put_string(props, pn_bytes(4,  "long"));
    pn_data_put_long(props, 12345);
    //
    //pn_data_put_string(props, pn_bytes(9, "timestamp"));
    pn_data_put_timestamp(props, (pn_timestamp_t) 54321);
    pn_data_exit(props);
#endif

    const int get_replies = opts.get_replies;

    if (get_replies) {
        // disable the timeout so that pn_messenger_recv() won't block
        reply_message = pn_message();
        check(reply_message, "failed to allocate a message");
    }

    statistics_start( &stats );
    while (!opts.msg_count || (sent < opts.msg_count)) {

        // setup the message to send
        pn_message_set_address(message, opts.targets.addresses[target_index]);
        target_index = NEXT_ADDRESS(opts.targets, target_index);
        id.u.as_ulong = sent;
        pn_message_set_correlation_id( message, id );
        pn_message_set_creation_time( message, msgr_now() );
        pn_messenger_put(messenger, message);
        sent++;
        if (opts.send_batch && (pn_messenger_outgoing(messenger) >= (int)opts.send_batch)) {
            if (get_replies) {
                while (received < sent) {
                    // this will also transmit any pending sent messages
                    received += process_replies( messenger, reply_message,
                                                 &stats, opts.recv_count );
                }
            } else {
                LOG("Calling pn_messenger_send()\n");
                rc = pn_messenger_send(messenger, -1);
                check((rc == 0 || rc == PN_TIMEOUT), "pn_messenger_send() failed");
            }
        }
        check_messenger(messenger);
    }

    LOG("Messages received=%" PRIu64 " sent=%" PRIu64 "\n", received, sent);

    if (get_replies) {
        // wait for the last of the replies
        while (received < sent) {
            int count = process_replies( messenger, reply_message,
                                         &stats, opts.recv_count );
            check( count > 0 || (opts.timeout == 0),
                   "Error: timed out waiting for reply messages\n");
            received += count;
            LOG("Messages received=%" PRIu64 " sent=%" PRIu64 "\n", received, sent);
        }
    } else if (pn_messenger_outgoing(messenger) > 0) {
        LOG("Calling pn_messenger_send()\n");
        rc = pn_messenger_send(messenger, -1);
        check(rc == 0, "pn_messenger_send() failed");
    }

    rc = pn_messenger_stop(messenger);
    check(rc == 0, "pn_messenger_stop() failed");
    check_messenger(messenger);

    statistics_report( &stats, sent, received );

    pn_messenger_free(messenger);
    pn_message_free(message);
    if (reply_message) pn_message_free( reply_message );
    addresses_free( &opts.targets );

    return 0;
}

#undef PN_USE_DEPRECATED_API
