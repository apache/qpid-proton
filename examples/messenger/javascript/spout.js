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
    //message.body = new proton.Data.Uuid();
    //message.body = new proton.Data.Symbol("My Symbol");
    //message.body = new proton.Data.Binary("Monkey Bathпогромзхцвбнм");
    //message.body = new proton.Data.Described("persian", "feline mammals");

    //message.body = new Date();

    //message.body = new proton.Data.Array('INT', [1, 3, 5, 7], "odd numbers");

    //message.body = new proton.Data.Array('UINT', [1, 3, 5, 7], "odd");
    //message.body = new proton.Data.Array('ULONG', [1, 3, 5, 7], "odd");
    //message.body = new proton.Data.Array('FLOAT', [1, 3, 5, 7], "odd");
    //message.body = new proton.Data.Array('STRING', ["1", "3", "5", "7"], "odd");

    //message.body = new Uint8Array([1, 3, 5, 7]);

    //message.body = new proton.Data.Array('UINT', new Uint8Array([1, 3, 5, 7]), "odd");

    //message.body = new proton.Data.Array('UUID', [new proton.Data.Uuid(), new proton.Data.Uuid(), new proton.Data.Uuid(), new proton.Data.Uuid()], "unique");

    /*message.body = new proton.Data.Binary(4);
    var buffer = message.body.getBuffer();
    buffer[0] = 65;
    buffer[1] = 77;
    buffer[2] = 81;
    buffer[3] = 80;*/
    message.body = new proton.Data.Binary([65, 77, 81, 80]);

    //message.body = null;
    //message.body = true;
    //message.body = 66..char();
    //message.body = "   \"127.0\"  ";

    //message.body = 2147483647; // int
    //message.body = -2147483649; // long
    //message.body = 12147483649; // long
    //message.body = (12147483649).long(); // long
    //message.body = (-12147483649).ulong(); // long
    //message.body = (17223372036854778000).ulong(); // ulong

    //message.body = (121474.836490).float(); // float TODO check me
    //message.body = 12147483649.0.float(); // float TODO check me
    //message.body = (4294967296).uint();
    //message.body = (255).ubyte();

    //message.body = ['Rod', 'Jane', 'Freddy'];
    //message.body = ['Rod', 'Jane', 'Freddy', {cat: true, donkey: 'hee haw'}];
    //message.body = {cat: true, donkey: 'hee haw'};

    tracker = messenger.put(message);

} catch(e) {
    console.log("Caught Exception " + e);
}
