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
    Addresses_t subscriptions;
    uint64_t msg_count;
    int recv_count;
    int incoming_window;
    int timeout;  // seconds
    unsigned int report_interval;  // in seconds
    int   outgoing_window;
    Addresses_t forwarding_targets;
    int   reply;
    const char *name;
    const char *ready_text;
    char *certificate;
    char *privatekey;   // used to sign certificate
    char *password;     // for private key file
    char *ca_db;        // trusted CA database
} Options_t;


static void usage(int rc)
{
    printf("Usage: msgr-recv [OPTIONS] \n"
           " -a <addr>[,<addr>]* \tAddresses to listen on [amqp://~0.0.0.0]\n"
           " -c # \tNumber of messages to receive before exiting [0=forever]\n"
           " -b # \tArgument to Messenger::recv(n) [2048]\n"
           " -w # \tSize for incoming window [0]\n"
           " -t # \tInactivity timeout in seconds, -1 = no timeout [-1]\n"
           " -e # \t# seconds to report statistics, 0 = end of test [0] *TBD*\n"
           " -R \tSend reply if 'reply-to' present\n"
           " -W # \t# outgoing window size [0]\n"
           " -F <addr>[,<addr>]* \tAddresses used for forwarding received messages\n"
           " -N <name> \tSet the container name to <name>\n"
           " -X <text> \tPrint '<text>\\n' to stdout after all subscriptions are created\n"
           " -V \tEnable debug logging\n"
           " SSL options:\n"
           " -T <path> \tDatabase of trusted CA certificates for validating peer\n"
           " -C <path> \tFile containing self-identifying certificate\n"
           " -K <path> \tFile containing private key used to sign certificate\n"
           " -P [pass:<password>|path] \tPassword to unlock private key file.\n"
           );
    exit(rc);
}

static void parse_options( int argc, char **argv, Options_t *opts )
{
    int c;
    opterr = 0;

    memset( opts, 0, sizeof(*opts) );
    opts->recv_count = -1;
    opts->timeout = -1;
    addresses_init(&opts->subscriptions);
    addresses_init(&opts->forwarding_targets);

    while ((c = getopt(argc, argv,
                       "a:c:b:w:t:e:RW:F:VN:X:T:C:K:P:")) != -1) {
        switch (c) {
        case 'a': addresses_merge( &opts->subscriptions, optarg ); break;
        case 'c':
            if (sscanf( optarg, "%" SCNu64, &opts->msg_count ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'b':
            if (sscanf( optarg, "%d", &opts->recv_count ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'w':
            if (sscanf( optarg, "%d", &opts->incoming_window ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 't':
            if (sscanf( optarg, "%d", &opts->timeout ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            if (opts->timeout > 0) opts->timeout *= 1000;
            break;
        case 'e':
            if (sscanf( optarg, "%u", &opts->report_interval ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'R': opts->reply = 1; break;
        case 'W':
            if (sscanf( optarg, "%d", &opts->outgoing_window ) != 1) {
                fprintf(stderr, "Option -%c requires an integer argument.\n", optopt);
                usage(1);
            }
            break;
        case 'F': addresses_merge( &opts->forwarding_targets, optarg ); break;
        case 'V': enable_logging(); break;
        case 'N': opts->name = optarg; break;
        case 'X': opts->ready_text = optarg; break;
        case 'T': opts->ca_db = optarg; break;
        case 'C': opts->certificate = optarg; break;
        case 'K': opts->privatekey = optarg; break;
        case 'P': parse_password( optarg, &opts->password ); break;

        default:
            usage(1);
        }
    }

    // default subscription if none specified
    if (opts->subscriptions.count == 0) addresses_add( &opts->subscriptions,
                                                       "amqp://~0.0.0.0" );
}



int main(int argc, char** argv)
{
    Options_t opts;
    Statistics_t stats;
    uint64_t sent = 0;
    uint64_t received = 0;
    int forwarding_index = 0;
    int rc;

    pn_message_t *message;
    pn_messenger_t *messenger;

    parse_options( argc, argv, &opts );

    const int forward = opts.forwarding_targets.count != 0;

    message = pn_message();
    messenger = pn_messenger( opts.name );

    /* load the various command line options if they're set */
    if (opts.certificate) {
        rc = pn_messenger_set_certificate(messenger, opts.certificate);
        check_messenger(messenger);
        check( rc == 0, "Failed to set certificate" );
    }

    if (opts.privatekey) {
        rc = pn_messenger_set_private_key(messenger, opts.privatekey);
        check_messenger(messenger);
        check( rc == 0, "Failed to set private key" );
    }

    if (opts.password) {
        rc = pn_messenger_set_password(messenger, opts.password);
        free(opts.password);
        check_messenger(messenger);
        check( rc == 0, "Failed to set password" );
    }

    if (opts.ca_db) {
        rc = pn_messenger_set_trusted_certificates(messenger, opts.ca_db);
        check_messenger(messenger);
        check( rc == 0, "Failed to set trusted CA database" );
    }

    if (opts.incoming_window) {
        // RAFI: seems to cause receiver to hang:
        pn_messenger_set_incoming_window( messenger, opts.incoming_window );
    }

    pn_messenger_set_timeout( messenger, opts.timeout );

    pn_messenger_start(messenger);
    check_messenger(messenger);

    int i;
    for (i = 0; i < opts.subscriptions.count; i++) {
        pn_messenger_subscribe(messenger, opts.subscriptions.addresses[i]);
        check_messenger(messenger);
        LOG("Subscribing to '%s'\n", opts.subscriptions.addresses[i]);
    }

    // hack to let test scripts know when the receivers are ready (so
    // that the senders may be started)
    if (opts.ready_text) {
        fprintf(stdout, "%s\n", opts.ready_text);
        fflush(stdout);
    }

    while (!opts.msg_count || received < opts.msg_count) {

        LOG("Calling pn_messenger_recv(%d)\n", opts.recv_count);
        rc = pn_messenger_recv(messenger, opts.recv_count);
        check_messenger(messenger);
        check(rc == 0 || (opts.timeout == 0 && rc == PN_TIMEOUT), "pn_messenger_recv() failed");

        // start the timer only after receiving the first msg
        if (received == 0) statistics_start( &stats );

        LOG("Messages on incoming queue: %d\n", pn_messenger_incoming(messenger));
        while (pn_messenger_incoming(messenger)) {
            pn_messenger_get(messenger, message);
            check_messenger(messenger);
            received++;
            // TODO: header decoding?
            // uint64_t id = pn_message_get_correlation_id( message ).u.as_ulong;
            statistics_msg_received( &stats, message );

            if (opts.reply) {
                const char *reply_addr = pn_message_get_reply_to( message );
                if (reply_addr) {
                    LOG("Replying to: %s\n", reply_addr );
                    pn_message_set_address( message, reply_addr );
                    pn_message_set_creation_time( message, msgr_now() );
                    pn_messenger_put(messenger, message);
                    sent++;
                }
            }

            if (forward) {
                const char *forward_addr = opts.forwarding_targets.addresses[forwarding_index];
                forwarding_index = NEXT_ADDRESS(opts.forwarding_targets, forwarding_index);
                LOG("Forwarding to: %s\n", forward_addr );
                pn_message_set_address( message, forward_addr );
                pn_message_set_reply_to( message, NULL );       // else points to origin sender
                pn_message_set_creation_time( message, msgr_now() );
                pn_messenger_put(messenger, message);
                sent++;
            }

        }
        LOG("Messages received=%" PRIu64 " sent=%" PRIu64 "\n", received, sent);
    }

    // this will flush any pending sends
    if (pn_messenger_outgoing(messenger) > 0) {
        LOG("Calling pn_messenger_send()\n");
        rc = pn_messenger_send(messenger, -1);
        check_messenger(messenger);
        check(rc == 0, "pn_messenger_send() failed");
    }

    rc = pn_messenger_stop(messenger);
    check(rc == 0, "pn_messenger_stop() failed");
    check_messenger(messenger);

    statistics_report( &stats, sent, received );

    pn_messenger_free(messenger);
    pn_message_free(message);
    addresses_free( &opts.subscriptions );
    addresses_free( &opts.forwarding_targets );

    return 0;
}

#undef PN_USE_DEPRECATED_API
