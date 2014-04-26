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
    var address = "amqp://0.0.0.0";
    var subject = "UK.WEATHER";
    var msgtext = "Hello World!";
    var tracker = null;
    var running = true;

    var message = new proton.Message();
    var messenger = new proton.Messenger();

    function _process() {
//        console.log("                          *** process ***");

        // Process outgoing messages
        var status = messenger.status(tracker);
        if (status != proton.Status.PENDING) {
console.log("status = " + status);

            //messenger.settle(tracker);
            //tracked--;

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
            //exit(0);
        }
    };


    messenger.setOutgoingWindow(1024);

    messenger.setNetworkCallback(_process);
    messenger.start();

    message.setAddress(address);
    message.setSubject(subject);
    //message.body = msgtext;
    //message.body = new proton.Data.UUID();
    //message.body = new proton.Data.Symbol("My Symbol");
    //message.body = new proton.Data.Binary("Monkey Bathпогромзхцвбнм");

    message.body = new proton.Data.Binary(4);
    var buffer = message.body.getBuffer();
    buffer[0] = 65;
    buffer[1] = 77;
    buffer[2] = 81;
    buffer[3] = 80;


    //message.body = true;
    //message.body = "   \"127.0\"  ";

    //message.body = 2147483647; // int
    //message.body = -2147483649; // long
    //message.body = 12147483649; // long

    //message.body = 2147483647.000001; // double

    //message.body = ['Rod', 'Jane', 'Freddy'];
    //message.body = ['Rod', 'Jane', 'Freddy', {cat: true, donkey: 'hee haw'}];


    tracker = messenger.put(message);

} catch(e) {
    console.log("Caught Exception " + e);
}
