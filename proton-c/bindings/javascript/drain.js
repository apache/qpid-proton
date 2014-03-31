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
    proton = require("../../../bld/proton-c/bindings/javascript/proton.js");
}

try {
    var address = "amqp://~0.0.0.0";
    var message = new proton.Message();
    var messenger = new proton.Messenger();

    function _process() {
//        console.log("                          *** process ***");

        // Process incoming messages

        while (messenger.incoming()) {
console.log("in while loop\n");

            var tracker = messenger.get(message);
console.log("tracker = " + tracker);

            console.log("Address: " + message.getAddress());
            console.log("Subject: " + message.getSubject());
            console.log("Content: " + message.body);

            messenger.accept(tracker);
        }
    };

    //messenger.setIncomingWindow(1024);

    messenger.setNetworkCallback(_process);
    messenger.start();

    messenger.subscribe(address);
    messenger.recv(); // Receive as many messages as messenger can buffer.
} catch(e) {
    console.log("Caught Exception " + e);
}


