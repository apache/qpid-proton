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

/**
 * Unit tests for the Data Class.
 * TODO this is just some random stuff at the moment - need to port the python codec test.
 */

// Check if the environment is Node.js and if so import the required library.
if (typeof exports !== "undefined" && exports !== null) {
    proton = require("../../../bld/proton-c/bindings/javascript/proton.js");
}


try {

    var messenger = new proton.Messenger();

    console.log("name = " + messenger.getName());

    console.log("timeout = " + messenger.getTimeout());

    console.log("isBlocking = " + messenger.isBlocking());

    messenger.setIncomingWindow(1234);
    console.log("incoming window = " + messenger.getIncomingWindow());

    messenger.setOutgoingWindow(5678);
    console.log("outgoing window = " + messenger.getOutgoingWindow());


    messenger.start();
    console.log("isStopped = " + messenger.isStopped());


    //messenger.subscribe("amqp://narnia");
    var subscription = messenger.subscribe("amqp://~0.0.0.0");
    console.log("subscription address = " + subscription.getAddress());


    var message = new proton.Message();
    message.setAddress("amqp://localhost:5672");
    console.log("message address = " + message.getAddress());

    message.setSubject("UK.WEATHER");
    console.log("message subject = " + message.getSubject());


    messenger.stop();
    console.log("isStopped = " + messenger.isStopped());



    message.free();

    messenger.free();

} catch(e) {
    console.log("Caught Exception " + e);
}
