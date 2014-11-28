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

// This is a re-implementation of recv.c using non-blocking/asynchronous calls.

#include "proton/message.h"
#include "proton/messenger.h"

#include "pncompat/misc_funcs.inc"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if EMSCRIPTEN
#include <emscripten.h>
#endif

pn_message_t * message;
pn_messenger_t * messenger;

#define check(messenger)                                                     \
  {                                                                          \
    if(pn_messenger_errno(messenger))                                        \
    {                                                                        \
      die(__FILE__, __LINE__, pn_error_text(pn_messenger_error(messenger))); \
    }                                                                        \
  }                                                                          \

void die(const char *file, int line, const char *message)
{
    fprintf(stderr, "%s:%i: %s\n", file, line, message);
    exit(1);
}

void usage(void)
{
    printf("Usage: recv [options] <addr>\n");
    printf("-c    \tPath to the certificate file.\n");
    printf("-k    \tPath to the private key file.\n");
    printf("-p    \tPassword for the private key.\n");
    printf("<addr>\tAn address.\n");
    exit(0);
}

void process(void) {
    while(pn_messenger_incoming(messenger))
    {
        pn_messenger_get(messenger, message);
        check(messenger);

        {
        pn_tracker_t tracker = pn_messenger_incoming_tracker(messenger);
        char buffer[1024];
        size_t buffsize = sizeof(buffer);
        const char* subject = pn_message_get_subject(message);
        pn_data_t* body = pn_message_body(message);
        pn_data_format(body, buffer, &buffsize);

        printf("Address: %s\n", pn_message_get_address(message));
        printf("Subject: %s\n", subject ? subject : "(no subject)");
        printf("Content: %s\n", buffer);

        pn_messenger_accept(messenger, tracker, 0);
        }
    }
}

#if EMSCRIPTEN // For emscripten C/C++ to JavaScript compiler.
void pump(int fd, void* userData) {
    while (pn_messenger_work(messenger, 0) >= 0) {
        process();
    }
}

void onclose(int fd, void* userData) {
    process();
}

void onerror(int fd, int errno, const char* msg, void* userData) {
    printf("error callback fd = %d, errno = %d, msg = %s\n", fd, errno, msg);
}
#endif

int main(int argc, char** argv)
{
    char* certificate = NULL;
    char* privatekey = NULL;
    char* password = NULL;
    char* address = (char *) "amqp://~0.0.0.0";
    int c;

    message = pn_message();
    messenger = pn_messenger(NULL);
    pn_messenger_set_blocking(messenger, false); // Needs to be set non-blocking to behave asynchronously.

    opterr = 0;

    while((c = getopt(argc, argv, "hc:k:p:")) != -1)
    {
        switch(c)
        {
            case 'h':
                usage();
                break;

            case 'c': certificate = optarg; break;
            case 'k': privatekey = optarg; break;
            case 'p': password = optarg; break;

            case '?':
                if (optopt == 'c' ||
                    optopt == 'k' ||
                    optopt == 'p')
                {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }
                else if(isprint(optopt))
                {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                else
                {
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    if (optind < argc)
    {
        address = argv[optind];
    }

    /* load the various command line options if they're set */
    if(certificate)
    {
        pn_messenger_set_certificate(messenger, certificate);
    }

    if(privatekey)
    {
        pn_messenger_set_private_key(messenger, privatekey);
    }

    if(password)
    {
        pn_messenger_set_password(messenger, password);
    }

    pn_messenger_start(messenger);
    check(messenger);

    pn_messenger_subscribe(messenger, address);
    check(messenger);

    pn_messenger_recv(messenger, -1); // Set to receive as many messages as messenger can buffer.

#if EMSCRIPTEN // For emscripten C/C++ to JavaScript compiler.
    emscripten_set_socket_error_callback(NULL, onerror);

    emscripten_set_socket_open_callback(NULL, pump);
    emscripten_set_socket_connection_callback(NULL, pump);
    emscripten_set_socket_message_callback(NULL, pump);
    emscripten_set_socket_close_callback(NULL, onclose);
#else // For native compiler.
    while (1) {
        pn_messenger_work(messenger, -1); // Block indefinitely until there has been socket activity.
        process();
    }
#endif

    return 0;
}

