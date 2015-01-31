#!/usr/bin/env node
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

// Check if the environment is Node.js and if not log an error and exit.
if (typeof process === 'object' && typeof require === 'function') {
    var usage =
    'Usage: msgr-send [OPTIONS]\n' +
    ' -a <addr>[,<addr>]* \tThe target address [amqp://~0.0.0.0]\n' +
    ' -c # \tNumber of messages to send before exiting [0=forever]\n' +
    ' -b # \tSize of message body in bytes [1024]\n' +
    ' -p # \tSend batches of # messages (wait for replies before sending next batch if -R) [1024]\n' +
    ' -w # \tSize for outgoing window [0]\n' +
    ' -e # \t# seconds to report statistics, 0 = end of test [0] *TBD*\n' +
    ' -R \tWait for a reply to each sent message\n' +
    ' -B # \tArgument to Messenger::recv(n) [-1]\n' +
    ' -N <name> \tSet the container name to <name>\n' +
    ' -V \tEnable debug logging\n';

    // Increase the virtual heap available to the emscripten compiled C runtime.
    // This allows us to test a really big string.
    PROTON_TOTAL_MEMORY = 140000000;
    PROTON_TOTAL_STACK = 25000000; // Needs to be bigger than the biggest string.
    var proton = require("qpid-proton-messenger");
    var benchmark = require("./msgr-send-common.js");

    var opts = {};
    opts.addresses = 'amqp://0.0.0.0';
    opts.messageCount = 0;
    opts.messageSize = 1024;
    opts.recvCount = -1;
    opts.sendBatch = 1024;
    opts.outgoingWindow;
    opts.reportInterval = 0;
    opts.getReplies = false;
    opts.name;
    opts.verbose = false;

    var args = process.argv.slice(2);
    if (args.length > 0) {
        if (args[0] === '-h' || args[0] === '--help') {
            console.log(usage);
            process.exit(0);
        }

        for (var i = 0; i < args.length; i++) {
            var arg = args[i];
            if (arg.charAt(0) === '-') {
                if (arg === '-V') {
                    opts.verbose = true;
                } else if (arg === '-R') {
                    opts.getReplies = true;
                } else {
                    i++;
                    var val = args[i];
                    if (arg === '-a') {
                        opts.addresses = val;
                    } else if (arg === '-c') {
                        opts.messageCount = val;
                    } else if (arg === '-b') {
                        opts.messageSize = val;
                    } else if (arg === '-B') {
                        opts.recvCount = val;
                    } else if (arg === '-p') {
                        opts.sendBatch = val;
                    } else if (arg === '-w') {
                        opts.outgoingWindow = val;
                    } else if (arg === '-e') {
                        opts.reportInterval = val;
                    } else if (arg === '-N') {
                        opts.name = val;
                    }
                }
            }
        }
    }

    var sender = new benchmark.MessengerSend(opts);
    sender.start();
} else {
    console.error("msgr-send.js should be run in Node.js");
}

