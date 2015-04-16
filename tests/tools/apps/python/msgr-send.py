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
Usage: msgr-send [OPTIONS]
 -a <addr>[,<addr>]* \tThe target address [amqp[s]://domain[/name]]
 -c # \tNumber of messages to send before exiting [0=forever]
 -b # \tSize of message body in bytes [1024]
 -p # \tSend batches of # messages (wait for replies before sending next batch if -R) [1024]
 -w # \t# outgoing window size [0]
 -e # \t# seconds to report statistics, 0 = end of test [0]
 -R \tWait for a reply to each sent message
 -t # \tInactivity timeout in seconds, -1 = no timeout [-1]
 -W # \tIncoming window size [0]
 -B # \tArgument to Messenger::recv(n) [-1]
 -N <name> \tSet the container name to <name>
 -V \tEnable debug logging"""


def parse_options( argv ):
    parser = optparse.OptionParser(usage=usage)
    parser.add_option("-a", dest="targets", action="append", type="string")
    parser.add_option("-c", dest="msg_count", type="int", default=0)
    parser.add_option("-b", dest="msg_size", type="int", default=1024)
    parser.add_option("-p", dest="send_batch", type="int", default=1024)
    parser.add_option("-w", dest="outgoing_window", type="int")
    parser.add_option("-e", dest="report_interval", type="int", default=0)
    parser.add_option("-R", dest="get_replies", action="store_true")
    parser.add_option("-t", dest="timeout", type="int", default=-1)
    parser.add_option("-W", dest="incoming_window", type="int")
    parser.add_option("-B", dest="recv_count", type="int", default=-1)
    parser.add_option("-N", dest="name", type="string")
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



def process_replies( messenger, message, stats, max_count, log):
    """
    Return the # of reply messages received
    """
    received = 0
    log.debug("Calling pn_messenger_recv(%d)", max_count)
    messenger.recv( max_count )
    log.debug("Messages on incoming queue: %d", messenger.incoming)
    while messenger.incoming > 0:
        messenger.get( message )
        received += 1
        # TODO: header decoding?
        stats.msg_received( message )
        # uint64_t id = pn_message_get_correlation_id( message ).u.as_ulong;
    return received

def main(argv=None):
    opts = parse_options(argv)[0]
    if opts.targets is None:
        opts.targets = ["amqp://0.0.0.0"]
    stats = Statistics()
    sent = 0
    received = 0
    target_index = 0

    log = logging.getLogger("msgr-send")
    log.addHandler(logging.StreamHandler())
    if opts.verbose:
        log.setLevel(logging.DEBUG)
    else:
        log.setLevel(logging.INFO)


    message = Message()
    message.reply_to = "~"
    message.body = "X" * opts.msg_size
    reply_message = Message()
    messenger = Messenger( opts.name )

    if opts.outgoing_window is not None:
        messenger.outgoing_window = opts.outgoing_window
    if opts.timeout > 0:
        opts.timeout *= 1000
    messenger.timeout = opts.timeout

    messenger.start()

    # unpack targets that were specified using comma-separated list
    #
    targets = []
    for x in opts.targets:
        z = x.split(",")
        for y in z:
            if y:
                targets.append(y)

    stats.start()
    while opts.msg_count == 0 or sent < opts.msg_count:
        # send a message
        message.address = targets[target_index]
        if target_index == len(targets) - 1:
            target_index = 0
        else:
            target_index += 1
        message.correlation_id = sent
        message.creation_time = long(time.time() * 1000)
        messenger.put( message )
        sent += 1

        if opts.send_batch and (messenger.outgoing >= opts.send_batch):
            if opts.get_replies:
                while received < sent:
                    # this will also transmit any pending sent messages
                    received += process_replies( messenger, reply_message,
                                                 stats, opts.recv_count, log )
            else:
                log.debug("Calling pn_messenger_send()")
                messenger.send()

    log.debug("Messages received=%d sent=%d", received, sent)

    if opts.get_replies:
        # wait for the last of the replies
        while received < sent:
            count = process_replies( messenger, reply_message, stats,
                                     opts.recv_count, log )
            received += count
            log.debug("Messages received=%d sent=%d", received, sent)

    elif messenger.outgoing > 0:
        log.debug("Calling pn_messenger_send()")
        messenger.send()

    messenger.stop()

    stats.report( sent, received )
    return 0

if __name__ == "__main__":
    sys.exit(main())
