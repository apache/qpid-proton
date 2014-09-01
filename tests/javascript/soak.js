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

var addr = 'guest:guest@localhost:5673';
//var addr = 'localhost:5673';
var address = 'amqp://' + addr;
console.log(address);

var subscriptionQueue = '';
var subscription;
var subscribed = false;
var count = 0;
var start = 0; // Start Time.

var message = new proton.Message();
var messenger = new proton.Messenger();

var pumpData = function() {
    if (!subscribed) {
        var subscriptionAddress = subscription.getAddress();
        if (subscriptionAddress) {
            subscribed = true;
            var splitAddress = subscriptionAddress.split('/');
            subscriptionQueue = splitAddress[splitAddress.length - 1];
            onSubscription();
        }
    }

    while (messenger.incoming()) {
        // The second parameter forces Binary payloads to be decoded as strings
        // this is useful because the broker QMF Agent encodes strings as AMQP
        // binary, which is a right pain from an interoperability perspective.
        var t = messenger.get(message, true);
        //console.log("Address: " + message.getAddress());
        //console.log("Content: " + message.body);
        messenger.accept(t);

        if (count % 1000 === 0) {
            var time = +new Date();
            console.log("count = " + count + ", duration = " + (time - start) + ", rate = " + ((count*1000)/(time - start)));
        }

        sendMessage();
    }

    if (messenger.isStopped()) {
        message.free();
        messenger.free();
    }
};

var sendMessage = function() {
    var msgtext = "Message Number " + count;
    count++;

    message.setAddress(address + '/' + subscriptionQueue);
    message.body = msgtext;
    messenger.put(message);
//messenger.settle();
};

messenger.on('error', function(error) {console.log(error);});
messenger.on('work', pumpData);
//messenger.setOutgoingWindow(1024);
messenger.setIncomingWindow(1024); // The Java Broker seems to need this.
messenger.start();

subscription = messenger.subscribe('amqp://' + addr + '/#');
messenger.recv(); // Receive as many messages as messenger can buffer.

var onSubscription = function() {
    console.log("Subscription Queue: " + subscriptionQueue);
    start = +new Date();
    sendMessage();
};

