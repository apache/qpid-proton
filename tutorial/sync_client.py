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

"""
Demonstrates the client side of the synchronous request-response pattern
(also known as RPC or Remote Procecure Call) using proton.

"""

from proton import Message, Url, ConnectionException, Timeout
from proton_events import BlockingConnection, IncomingMessageHandler
import sys

class SyncRequestClient(IncomingMessageHandler):
    """
    Implementation of the synchronous request-responce (aka RPC) pattern.
    Create an instance and call invoke() to send a request and wait for a response.
    """

    def __init__(self, url, timeout=None):
        """
        @param url: a proton.Url or a URL string of the form 'host:port/path'
            host:port is used to connect, path is used to identify the remote messaging endpoint.
        """
        self.connection = BlockingConnection(Url(url).defaults(), timeout=timeout)
        self.sender = self.connection.sender(url.path)
        # dynamic=true generates a unique address dynamically for this receiver.
        # credit=1 because we want to receive 1 response message initially.
        self.receiver = self.connection.receiver(None, dynamic=True, credit=1, handler=self)
        self.response = None

    def invoke(self, request):
        """Send a request, wait for and return the response"""
        request.reply_to = self.reply_to
        self.sender.send_msg(request)
        self.connection.wait(lambda: self.response, msg="Waiting for response")
        response = self.response
        self.response = None    # Ready for next response.
        self.receiver.flow(1)   # Set up credit for the next response.
        return response

    @property
    def reply_to(self):
        """Return the dynamic address of our receiver."""
        return self.receiver.remote_source.address

    def on_message(self, event):
        """Called when we receive a message for our receiver."""
        self.response = event.message # Store the response

    def close(self):
        self.connection.close()


if __name__ == '__main__':
    url = Url("0.0.0.0/examples")
    if len(sys.argv) > 1: url = Url(sys.argv[1])

    invoker = SyncRequestClient(url, timeout=2)
    try:
        REQUESTS= ["Twas brillig, and the slithy toves",
                   "Did gire and gymble in the wabe.",
                   "All mimsy were the borogroves,",
                   "And the mome raths outgrabe."]
        for request in REQUESTS:
            response = invoker.invoke(Message(body=request))
            print "%s => %s" % (request, response.body)
    finally:
        invoker.close()
