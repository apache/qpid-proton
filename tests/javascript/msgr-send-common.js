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
 * This file is essentially a "module" that is common to msgr-send.js and msgr-send.html.
 * It defines the Statistics and MessengerSend classes and if the environment is Node.js
 * it will import qpid-proton-messenger and export MessengerSend for use in msgr-send.js.
 * Because it's really a module/library trying to execute msgr-send-common.js won't
 * itself do anything terribly exciting.
 */

var Statistics = function() {
    this.startTime = 0;
    this.latencySamples = 0;
    this.latencyTotal = 0;
    this.latencyMin = 0;
    this.latencyMax = 0;
};

Statistics.prototype.start = function() {
    this.startTime = +new Date();
};

Statistics.prototype.messageReceived = function(msg) {
    var ts = +msg.getCreationTime(); // The + gets the value of the returned Data Object.
    if (ts) {
        var l = +new Date() - ts;
        if (l) {
            this.latencyTotal += l;
            this.latencySamples += 1;
            if (this.latencySamples === 1) {
                this.latencyMin = this.latencyMax = l;
            } else {
                if (this.latencyMin > l) {
                    this.latencyMin = l;
                }
                if (this.latencyMax < l) {
                    this.latencyMax = l;
                }
            }
        }
    }
};

Statistics.prototype.report = function(sent, received) {
    var seconds = (+new Date() - this.startTime)/1000;
    console.log("Messages sent: " + sent + " received: " + received);
    console.log("Total time: " + seconds + " seconds");
    if (seconds) {
        console.log("Throughput: " + (sent/seconds) + " msgs/sec sent");
        console.log("Throughput: " + (received/seconds) + " msgs/sec received");
    }

    if (this.latencySamples) {
        console.log("Latency (ms): " + this.latencyMin + " min " + 
                                       this.latencyMax + " max " + 
                                       (this.latencyTotal/this.latencySamples) + " avg");
    }
};


var MessengerSend = function(opts, callback) {
    //if (opts.verbose) {
        console.log("addresses = " + opts.addresses);
        console.log("messageCount = " + opts.messageCount);
        console.log("messageSize = " + opts.messageSize);
        console.log("recvCount = " + opts.recvCount);
        console.log("sendBatch = " + opts.sendBatch);
        console.log("outgoingWindow = " + opts.outgoingWindow);
        console.log("reportInterval = " + opts.reportInterval);
        console.log("getReplies = " + opts.getReplies);
        console.log("name = " + opts.name);
        console.log("verbose = " + opts.verbose);
        console.log();
    //}

    var stats = new Statistics();
    var targets = [];
    var running = true; // Used to avoid calling stop multiple times.
    var sent = 0;
    var received = 0;

    var message = new proton.Message();
    var replyMessage = new proton.Message();
    var messenger = new proton.Messenger(opts.name);

    // Retrieve replies and return the number of reply messages received.
    var processReplies = function() {
        var received = 0;
        if (opts.verbose) {
            console.log("Calling messenger.recv(" + opts.recvCount + ")");
        }
        messenger.recv(opts.recvCount);

        if (opts.verbose) {
            console.log("Messages on incoming queue: " + messenger.incoming());
        }
        while (messenger.incoming()) {
            messenger.get(replyMessage);
            received += 1;
            //console.log("Address: " + replyMessage.getAddress());
            //console.log("Content: " + replyMessage.body);
            stats.messageReceived(replyMessage);
        }
        return received;
    };

    // Send messages as fast as possible. This is analogous to the while loop in
    // the Python msgr-send.py but we wrap in a function in JavaScript so that
    // we can schedule on the JavaScript Event queue via setTimeout. This is needed
    // otherwise the JavaScript Event loop is blocked and no data gets sent.
    var sendData = function() {
        var delay = 0;
        while (opts.messageCount === 0 || (sent < opts.messageCount)) {
            // Check the amount of data buffered on the socket, if it's non-zero
            // exit the loop and call senData again after a short delay. This
            // will throttle the send rate if necessary.
            if (messenger.getBufferedAmount()) {
console.log("messenger.getBufferedAmount() = " + messenger.getBufferedAmount());
                delay = 100;
                break; // Exit loop to check for exit condition and schedule to Event queue.
            }       

            var index = sent % targets.length;
//console.log("sent = " + sent + ", index = " + index);

            message.setAddress(targets[index]);
            message.setCorrelationID(sent);
            message.setCreationTime(new Date());
            messenger.put(message);
            sent += 1;

            if (opts.sendBatch && (messenger.outgoing() >= opts.sendBatch)) {
                if (opts.verbose) {
                    console.log("Calling messenger.send()")
                }
                messenger.send();

                if (opts.getReplies) {
                    received += processReplies();
                }
                break; // Exit loop to check for exit condition and yield to Event loop.
            }
        }

        // Check for exit condition.
        if (running && !(opts.messageCount === 0 || (sent < opts.messageCount))) {
            if (opts.getReplies && (received < sent)) {
                received += processReplies();
                if (opts.verbose) {
                    console.log("Messages sent = " + sent + ", received = " + received);
                }
            } else if (messenger.outgoing()) {
                if (opts.verbose) {
                    console.log("Flushing pending sends");
                }
                messenger.send();
            } else {
//console.log("******* calling stop")
                messenger.stop();
                running = false;
                stats.report(sent, received);
                if (callback) {
                    callback(stats);
                }
            }
        }

        if (messenger.isStopped()) {
//console.log("-------------------- messenger.isStopped()");
            message.free();
            messenger.free();
        } else {
            // schedule next call on the JavaScript Event queue. If we don't do this
            // our messages won't get sent because none of the internal JavaScript
            // network code will get any CPU.

            // If available we call setImmediate rather than setTimeout when the delay
            // is zero. setImmediate is more efficient, in particular I noticed that
            // with Node.js v0.10.18 I could get max throughput and max out CPU using
            // setTimeout, but when I upgraded to v0.10.33 my throughput dropped and
            // my CPU was hovering around 55% but using setImmediate the performance
            // improved again. My guess is that v0.10.18 was checking for zero delay
            // and calling setImmediate internally whereas v0.10.33 wasn't, but I
            // can't say for sure. TODO it's possible that some browsers might do a
            // better job with setImmediate too (given what I'm seeing with Node.js),
            // Chrome might be one such case, but it's not universally supported.
            // It might be worth adding a proper polyfill to handle this.
            if (delay === 0 && typeof setImmediate === 'function') {
                setImmediate(sendData);
            } else {
                setTimeout(sendData, delay);
            }   
        }
    };

    this.start = function() {
        message.body = Array(+opts.messageSize + 1).join('X');
        message.setReplyTo('~');

        messenger.on('error', function(error) {
            console.log(error);
            opts.messageCount = -1; // Force exit condition.
        });

        if (opts.outgoingWindow) {
            messenger.setOutgoingWindow(opts.outgoingWindow);
        }
        messenger.start();

        // Unpack targets that were specified using comma-separated list
        var addresses = opts.addresses.split(',');
        for (var i = 0; i < addresses.length; i++) {
            var address = addresses[i];
            targets.push(address);
        }

        stats.start();
        sendData();
    };
};

// If running in Node.js import the proton library and export MessengerSend.
if (typeof module === 'object') {
    var proton = require("qpid-proton-messenger");
    module.exports.MessengerSend = MessengerSend;
}

