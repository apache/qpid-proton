#!/usr/bin/env python

#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
from __future__ import print_function
import sys, optparse, time
import logging
from proton import *



usage = """
Usage: msgr-recv [OPTIONS]
 -a <addr>[,<addr>]* \tAddresses to listen on [amqp://~0.0.0.0]
 -c # \tNumber of messages to receive before exiting [0=forever]
 -b # \tArgument to Messenger::recv(n) [2048]
 -w # \tSize for incoming window [0]
 -t # \tInactivity timeout in seconds, -1 = no timeout [-1]
 -e # \t# seconds to report statistics, 0 = end of test [0] *TBD*
 -R \tSend reply if 'reply-to' present
 -W # \t# outgoing window size [0]
 -F <addr>[,<addr>]* \tAddresses used for forwarding received messages
 -N <name> \tSet the container name to <name>
 -X <text> \tPrint '<text>\\n' to stdout after all subscriptions are created
 -V \tEnable debug logging"""


def parse_options( argv ):
    parser = optparse.OptionParser(usage=usage)
    parser.add_option("-a", dest="subscriptions", action="append", type="string")
    parser.add_option("-c", dest="msg_count", type="int", default=0)
    parser.add_option("-b", dest="recv_count", type="int", default=-1)
    parser.add_option("-w", dest="incoming_window", type="int")
    parser.add_option("-t", dest="timeout", type="int", default=-1)
    parser.add_option("-e", dest="report_interval", type="int", default=0)
    parser.add_option("-R", dest="reply", action="store_true")
    parser.add_option("-W", dest="outgoing_window", type="int")
    parser.add_option("-F", dest="forwarding_targets", action="append", type="string")
    parser.add_option("-N", dest="name", type="string")
    parser.add_option("-X", dest="ready_text", type="string")
    parser.add_option("-V", dest="verbose", action="store_true")

    return parser.parse_args(args=argv)


class Statistics(object):
    def __init__(self):
        self.start_time = 0.0
        self.latency_samples = 0
        self.latency_total = 0.0
        self.latency_min = None
        self.latency_max = None

    def start(self):
        self.start_time = time.time()

    def msg_received(self, msg):
        ts = msg.creation_time
        if ts:
            l = long(time.time() * 1000) - ts
            if l > 0.0:
                self.latency_total += l
                self.latency_samples += 1
                if self.latency_samples == 1:
                    self.latency_min = self.latency_max = l
                else:
                    if self.latency_min > l:
                        self.latency_min = l
                    if self.latency_max < l:
                        self.latency_max = l

    def report(self, sent, received):
        secs = time.time() - self.start_time
        print("Messages sent: %d recv: %d" % (sent, received) )
        print("Total time: %f sec" % secs )
        if secs:
            print("Throughput: %f msgs/sec" % (sent/secs) )
        if self.latency_samples:
            print("Latency (sec): %f min %f max %f avg" % (self.latency_min/1000.0,
                                                           self.latency_max/1000.0,
                                                           (self.latency_total/self.latency_samples)/1000.0))


def main(argv=None):
    opts = parse_options(argv)[0]
    if opts.subscriptions is None:
        opts.subscriptions = ["amqp://~0.0.0.0"]
    stats = Statistics()
    sent = 0
    received = 0
    forwarding_index = 0

    log = logging.getLogger("msgr-recv")
    log.addHandler(logging.StreamHandler())
    if opts.verbose:
        log.setLevel(logging.DEBUG)
    else:
        log.setLevel(logging.INFO)

    message = Message()
    messenger = Messenger( opts.name )

    if opts.incoming_window is not None:
        messenger.incoming_window = opts.incoming_window
    if opts.timeout > 0:
        opts.timeout *= 1000
    messenger.timeout = opts.timeout

    messenger.start()

    # unpack addresses that were specified using comma-separated list

    for x in opts.subscriptions:
        z = x.split(",")
        for y in z:
            if y:
                log.debug("Subscribing to %s", y)
                messenger.subscribe(y)

    forwarding_targets = []
    if opts.forwarding_targets:
        for x in opts.forwarding_targets:
            z = x.split(",")
            for y in z:
                if y:
                    forwarding_targets.append(y)

    # hack to let test scripts know when the receivers are ready (so that the
    # senders may be started)
    if opts.ready_text:
        print("%s" % opts.ready_text)
        sys.stdout.flush()

    while opts.msg_count == 0 or received < opts.msg_count:

        log.debug("Calling pn_messenger_recv(%d)", opts.recv_count)
        rc = messenger.recv(opts.recv_count)

        # start the timer only after receiving the first msg
        if received == 0:
            stats.start()

        log.debug("Messages on incoming queue: %d", messenger.incoming)
        while messenger.incoming > 0:
            messenger.get(message)
            received += 1
            # TODO: header decoding?
            # uint64_t id = pn_message_get_correlation_id( message ).u.as_ulong;
            stats.msg_received( message )

            if opts.reply:
                if message.reply_to:
                    log.debug("Replying to: %s", message.reply_to )
                    message.address = message.reply_to
                    message.creation_time = long(time.time() * 1000)
                    messenger.put( message )
                    sent += 1

            if forwarding_targets:
                forward_addr = forwarding_targets[forwarding_index]
                forwarding_index += 1
                if forwarding_index == len(forwarding_targets):
                    forwarding_index = 0
                log.debug("Forwarding to: %s", forward_addr )
                message.address = forward_addr
                message.reply_to = None
                message.creation_time = long(time.time() * 1000)
                messenger.put( message )
                sent += 1

        log.debug("Messages received=%lu sent=%lu", received, sent)

    # this will flush any pending sends
    if messenger.outgoing > 0:
        log.debug("Calling pn_messenger_send()")
        messenger.send()

    messenger.stop()

    stats.report( sent, received )
    return 0


if __name__ == "__main__":
  sys.exit(main())
