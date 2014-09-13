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
if (!exports) {
    console.error("recv.js should be run in Node.js");
    return;
}

var proton = require("qpid-proton");

var address = "amqp://~0.0.0.0";
var message = new proton.Message();
var messenger = new proton.Messenger();

var pumpData = function() {
    while (messenger.incoming()) {
        var t = messenger.get(message);

        console.log("Address: " + message.getAddress());
        console.log("Subject: " + message.getSubject());

        // body is the body as a native JavaScript Object, useful for most real cases.
        //console.log("Content: " + message.body);

        // data is the body as a proton.Data Object, used in this case because
        // format() returns exactly the same representation as recv.c
        console.log("Content: " + message.data.format());

        messenger.accept(t);
    }
};

var args = process.argv.slice(2);
if (args.length > 0) {
    if (args[0] === '-h' || args[0] === '--help') {
        console.log("Usage: node recv.js <addr> (default " + address + ")");
        process.exit(0);
    }

    address = args[0];
}

messenger.setIncomingWindow(1024);

messenger.on('error', function(error) {console.log(error);});
messenger.on('work', pumpData);
messenger.start();

messenger.subscribe(address);
messenger.recv(); // Receive as many messages as messenger can buffer.

