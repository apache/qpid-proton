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

// Simple client for use with server.js illustrating request/response

// Check if the environment is Node.js and if so import the required library.
if (typeof exports !== "undefined" && exports !== null) {
    proton = require("qpid-proton");
}

var address = "amqp://0.0.0.0";
var subject = "UK.WEATHER";
var replyTo = "~/replies";
var msgtext = "Hello World!";
var tracker = null;
var running = true;

var message = new proton.Message();
var messenger = new proton.Messenger();

var pumpData = function() {
    while (messenger.incoming()) {
        var t = messenger.get(message);

        console.log("Reply");
        console.log("Address: " + message.getAddress());
        console.log("Subject: " + message.getSubject());

        // body is the body as a native JavaScript Object, useful for most real cases.
        //console.log("Content: " + message.body);

        // data is the body as a proton.Data Object, used in this case because
        // format() returns exactly the same representation as recv.c
        console.log("Content: " + message.data.format());

        messenger.accept(t);
        messenger.stop();
    }

    if (messenger.isStopped()) {
        message.free();
        messenger.free();
    }
};

var args = process.argv.slice(2);
if (args.length > 0) {
    if (args[0] === '-h' || args[0] === '--help') {
        console.log("Usage: node client.js [-r replyTo] [-s subject] <addr> (default " + address + ")");
        console.log("Options:");
        console.log("  -r <reply to> The message replyTo (default " + replyTo + ")");
        console.log("  -s <subject> The message subject (default " + subject + ")");
        process.exit(0);
    }

    for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        if (arg.charAt(0) === '-') {
            i++;
            var val = args[i];
            if (arg === '-r') {
                replyTo = val;
            } else if (arg === '-s') {
                subject = val;
            }
        } else {
            address = arg;
        }
    }
}

messenger.on('error', function(error) {console.log(error);});
messenger.on('work', pumpData);
messenger.setOutgoingWindow(1024);
messenger.start();

message.setAddress(address);
message.setSubject(subject);
message.setReplyTo(replyTo);
message.body = msgtext;

tracker = messenger.put(message);
messenger.recv(); // Receive as many messages as messenger can buffer.

