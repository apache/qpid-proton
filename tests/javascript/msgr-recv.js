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


var MessengerReceive = function(opts, callback) {
    //if (opts.verbose) {
        console.log("subscriptions = " + opts.subscriptions);
        console.log("messageCount = " + opts.messageCount);
        console.log("recvCount = " + opts.recvCount);
        console.log("incomingWindow = " + opts.incomingWindow);
        console.log("reportInterval = " + opts.reportInterval);
        console.log("reply = " + opts.reply);
        console.log("forwardingTargets = " + opts.forwardingTargets);
        console.log("name = " + opts.name);
        console.log("readyText = " + opts.readyText);
        console.log("verbose = " + opts.verbose);
        console.log();
    //}

    var stats = new Statistics();
    var running = true; // Used to avoid calling stop multiple times.
    var sent = 0;
    var received = 0;
    var forwardingIndex = 0;

    var message = new proton.Message();
    var messenger = new proton.Messenger(opts.name);

    var pumpData = function() {
        if (opts.verbose) {
            console.log("Calling messenger.recv(" + opts.recvCount + ")");
        }
        messenger.recv(opts.recvCount);

        if (opts.verbose) {
            console.log("Messages on incoming queue: " + messenger.incoming());
        }
        while (messenger.incoming()) {
            // start the timer only after receiving the first msg
            if (received === 0) {
                stats.start();
            }

            messenger.get(message);
            received += 1;
            //console.log("Address: " + message.getAddress());
            //console.log("CorrelationID: " + message.getCorrelationID());
            //console.log("Content: " + message.body);
            stats.messageReceived(message);

            if (opts.reply) {
                var replyTo = message.getReplyTo();
                if (replyTo) {
                    if (opts.verbose) {
                        console.log("Replying to: " + replyTo);
                    }
                    message.setAddress(replyTo);
                    message.setCreationTime(new Date());
                    messenger.put(message);
                    sent += 1;
                }
            }
        }

        // Check for exit condition.
        if (running && !(opts.messageCount === 0 || received < opts.messageCount)) {
            // Wait for outgoing to be zero before calling stop so pending sends
            // get flushed properly.
            if (messenger.outgoing()) {
                if (opts.verbose) {
                    console.log("Flushing pending sends");
                }
            } else {
//console.log("******* messenger.stop()");
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
        }
    };

    this.start = function() {
        messenger.on('error', function(error) {console.log("** error **"); console.log(error);});
        messenger.on('work', pumpData);
        messenger.on('subscription', function(subscription) {
            // Hack to let test scripts know when the receivers are ready (so that the
            // senders may be started).
console.log("****** subscription " + subscription.getAddress() + " succeeded")
            if (opts.readyText) {
                console.log(opts.readyText);
            }
        });

        if (opts.incomingWindow) {
            messenger.setIncomingWindow(opts.incomingWindow);
        }
        messenger.start();

        // Unpack addresses that were specified using comma-separated list
        var subscriptions = opts.subscriptions.split(',');
        for (var i = 0; i < subscriptions.length; i++) {
            var subscription = subscriptions[i];
            if (opts.verbose) {
                console.log("Subscribing to " + subscription);
            }
            messenger.subscribe(subscription);
        }
    };
};


// Check if the environment is Node.js and if not log an error and exit.
if (typeof process === 'object' && typeof require === 'function') {
    var usage =
    'Usage: msgr-recv [OPTIONS]\n' +
    ' -a <addr>[,<addr>]* \tAddresses to listen on [amqp://~0.0.0.0]\n' +
    ' -c # \tNumber of messages to receive before exiting [0=forever]\n' +
    ' -b # \tArgument to Messenger::recv(n) [2048]\n' +
    ' -w # \tSize for incoming window [0]\n' +
    ' -e # \t# seconds to report statistics, 0 = end of test [0] *TBD*\n' +
    ' -R \tSend reply if \'reply-to\' present\n' +
    ' -F <addr>[,<addr>]* \tAddresses used for forwarding received messages\n' +
    ' -N <name> \tSet the container name to <name>\n' +
    ' -X <text> \tPrint \'<text>\\n\' to stdout after all subscriptions are created\n' +
    ' -V \tEnable debug logging\n';

    // Increase the virtual heap available to the emscripten compiled C runtime.
    // This allows us to test a really big string.
    PROTON_TOTAL_MEMORY = 140000000;
    PROTON_TOTAL_STACK = 25000000; // Needs to be bigger than the biggest string.
    var proton = require("qpid-proton-messenger");

    var opts = {};
    opts.subscriptions = 'amqp://~0.0.0.0';
    opts.messageCount = 0;
    opts.recvCount = -1;
    opts.incomingWindow;
    opts.reportInterval = 0;
    opts.reply = false;
    opts.forwardingTargets;
    opts.name;
    opts.readyText;
    opts.verbose = false;

    var args = process.argv.slice(2);
    if (args.length > 0) {
        if (args[0] === '-h' || args[0] === '--help') {
            console.log(usage);
            process.exit(0);
        }

        for (var i = 0; i < args.length; i++) {
            var arg = args[i];
            if (arg.charAt(0) === '-') {
                if (arg === '-V') {
                    opts.verbose = true;
                } else if (arg === '-R') {
                    opts.reply = true;
                } else {
                    i++;
                    var val = args[i];
                    if (arg === '-a') {
                        opts.subscriptions = val;
                    } else if (arg === '-c') {
                        opts.messageCount = val;
                    } else if (arg === '-b') {
                        opts.recvCount = val;
                    } else if (arg === '-w') {
                        opts.incomingWindow = val;
                    } else if (arg === '-e') {
                        opts.reportInterval = val;
                    } else if (arg === '-F') {
                        opts.forwardingTargets = val;
                    } else if (arg === '-N') {
                        opts.name = val;
                    } else if (arg === '-X') {
                        opts.readyText = val;
                    }
                }
            }
        }
    }

    var receiver = new MessengerReceive(opts);
    receiver.start();
} else {
    console.error("msgr-recv.js should be run in Node.js");
}

