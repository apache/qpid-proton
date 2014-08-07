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

// Check if the environment is Node.js and if so import the required library.
if (typeof exports !== "undefined" && exports !== null) {
    proton = require("qpid-proton");
}

var address = "amqp://0.0.0.0";
var subject = "UK.WEATHER";
var msgtext = "Hello World!";
var tracker = null;
var running = true;

var message = new proton.Message();
var messenger = new proton.Messenger();

var pumpData = function() {
    var status = messenger.status(tracker);
    if (status != proton.Status.PENDING) {
        if (running) {
            messenger.stop();
            running = false;
        } 
    }

    if (messenger.isStopped()) {
        message.free();
        messenger.free();
    }
};

var args = process.argv.slice(2);
if (args.length > 0) {
    if (args[0] === '-h' || args[0] === '--help') {
        console.log("Usage: node send.js [options] [message]");
        console.log("Options:");
        console.log("  -a <addr> The target address [amqp[s]://domain[/name]] (default " + address + ")");
        console.log("  -s <subject> The message subject (default " + subject + ")");
        console.log("message A text string to send.");
        process.exit(0);
    }

    for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        if (arg.charAt(0) === '-') {
            i++;
            var val = args[i];
            if (arg === '-a') {
                address = val;
            } else if (arg === '-s') {
                subject = val;
            }
        } else {
            msgtext = arg;
        }
    }
}

console.log("Address: " + address);
console.log("Subject: " + subject);
console.log("Content: " + msgtext);

messenger.on('error', function(error) {console.log(error);});
messenger.on('work', pumpData);
messenger.setOutgoingWindow(1024);
messenger.start();

message.setAddress(address);
message.setSubject(subject);
message.body = msgtext;

tracker = messenger.put(message);

