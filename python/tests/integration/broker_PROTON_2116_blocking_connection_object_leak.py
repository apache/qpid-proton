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
# under the License
#

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import optparse
import socket
import sys
import threading

import cproton

import proton.handlers
import proton.reactor
import proton.utils


class Broker(proton.handlers.MessagingHandler):
    def __init__(self, acceptor_url):
        # type: (str) -> None
        super(Broker, self).__init__()
        self.acceptor_url = acceptor_url

        self.acceptor = None
        self._acceptor_opened_event = threading.Event()

    def get_acceptor_sockname(self):
        # type: () -> (str, int)
        self._acceptor_opened_event.wait()
        if hasattr(self.acceptor, '_selectable'):  # proton 0.30.0+
            sockname = self.acceptor._selectable._delegate.getsockname()
        else:  # works in proton 0.27.0
            selectable = cproton.pn_cast_pn_selectable(self.acceptor._impl)
            fd = cproton.pn_selectable_get_fd(selectable)
            s = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
            sockname = s.getsockname()
        return sockname[:2]

    def on_start(self, event):
        self.acceptor = event.container.listen(self.acceptor_url)
        self._acceptor_opened_event.set()

    def on_link_opening(self, event):
        if event.link.is_sender:
            assert not event.link.remote_source.dynamic, "This cannot happen"
            event.link.source.address = event.link.remote_source.address
        elif event.link.remote_target.address:
            event.link.target.address = event.link.remote_target.address


def main():
    parser = optparse.OptionParser()
    parser.add_option("-b", dest="hostport", default="localhost:0", type="string",
                      help="port number to use")
    options, args = parser.parse_args()

    broker = Broker(options.hostport)
    container = proton.reactor.Container(broker)
    threading.Thread(target=container.run).start()
    print("{0}:{1}".format(*broker.get_acceptor_sockname()))
    sys.stdout.flush()


if __name__ == '__main__':
    main()
