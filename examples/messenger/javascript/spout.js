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
    var proton = require("qpid-proton");

    console.log("spout not implemented yet");
    process.exit(0);
    
    var address = "amqp://0.0.0.0";
    var subject = "UK.WEATHER";
    var msgtext = "Hello World!";
    var tracker = null;
    var running = true;
    
    var message = new proton.Message();
    var messenger = new proton.Messenger();

    function pumpData() {
        var status = messenger.status(tracker);
        if (status != proton.Status.PENDING) {
console.log("status = " + status);

            if (running) {
console.log("stopping");
                messenger.stop();
                running = false;
            } 
        }
    
        if (messenger.isStopped()) {
console.log("exiting");
            message.free();
            messenger.free();
        }
    };

    messenger.on('error', function(error) {console.log(error);});
    messenger.on('work', pumpData);
    messenger.setOutgoingWindow(1024);
    messenger.start();

    message.setAddress(address);
    message.setSubject(subject);

    message.body = msgtext;

    tracker = messenger.put(message);
} else {
    console.error("spout.js should be run in Node.js");
}

