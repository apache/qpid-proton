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
import os, common
from subprocess import Popen,PIPE
from proton import *
import sys

#
# Classes that wrap the messenger applications
#

class MessengerApp(object):
    """ Interface to control a MessengerApp """
    def __init__(self):
        self._cmdline = None

    def start(self, verbose=False):
        """ Begin executing the test """
        if sys.platform.startswith("java"):
            raise common.Skipped("Skipping soak tests - not supported under Jython")
        cmd = self.cmdline()
        self._verbose = verbose
        if self._verbose:
            print("COMMAND='%s'" % str(cmd))
        #print("ENV='%s'" % str(os.environ.copy()))
        try:
            self._process = Popen(cmd, stdout=PIPE, bufsize=4096)
        except OSError, e:
            assert False, "Unable to execute command '%s', is it in your PATH?" % cmd[0]
        self._ready()  # wait for it to initialize

    def stop(self):
        """ Signal the client to start clean shutdown """
        pass

    def wait(self):
        """ Wait for client to complete """
        self._output = self._process.communicate()
        if self._verbose:
            print("OUTPUT='%s'" % self.stdout())

    def status(self):
        """ Return status from client process """
        return self._process.returncode

    def stdout(self):
        #self._process.communicate()[0]
        return self._output[0]

    def cmdline(self):
        if not self._cmdline:
            self._build_command()
        return self._cmdline

    def _build_command(self):
        assert False, "_build_command() needs override"

    def _ready(self):
        assert False, "_ready() needs override"



class MessengerSender(MessengerApp):
    """ Interface to configure a sending MessengerApp """
    def __init__(self):
        MessengerApp.__init__(self)
        self._command = None
        # @todo make these properties
        self.targets = []
        self.send_count = None
        self.msg_size = None
        self.send_batch = None
        self.outgoing_window = None
        self.report_interval = None
        self.get_reply = False
        self.timeout = None
        self.incoming_window = None
        self.recv_count = None
        self.name = None

    # command string?
    def _build_command(self):
        self._cmdline = self._command
        assert self.targets, "Missing targets, required for sender!"
        self._cmdline.append("-a")
        self._cmdline.append(",".join(self.targets))
        if self.send_count is not None:
            self._cmdline.append("-c")
            self._cmdline.append(str(self.send_count))
        if self.msg_size is not None:
            self._cmdline.append("-b")
            self._cmdline.append(str(self.msg_size))
        if self.send_batch is not None:
            self._cmdline.append("-p")
            self._cmdline.append(str(self.send_batch))
        if self.outgoing_window is not None:
            self._cmdline.append("-w")
            self._cmdline.append(str(self.outgoing_window))
        if self.report_interval is not None:
            self._cmdline.append("-e")
            self._cmdline.append(str(self.report_interval))
        if self.get_reply:
            self._cmdline.append("-R")
        if self.timeout is not None:
            self._cmdline.append("-t")
            self._cmdline.append(str(self.timeout))
        if self.incoming_window is not None:
            self._cmdline.append("-W")
            self._cmdline.append(str(self.incoming_window))
        if self.recv_count is not None:
            self._cmdline.append("-B")
            self._cmdline.append(str(self.recv_count))
        if self.name is not None:
            self._cmdline.append("-N")
            self._cmdline.append(str(self.name))

    def _ready(self):
        pass


class MessengerReceiver(MessengerApp):
    """ Interface to configure a receiving MessengerApp """
    def __init__(self):
        MessengerApp.__init__(self)
        self._command = None
        # @todo make these properties
        self.subscriptions = []
        self.receive_count = None
        self.recv_count = None
        self.incoming_window = None
        self.timeout = None
        self.report_interval = None
        self.send_reply = False
        self.outgoing_window = None
        self.forwards = []
        self.name = None

    # command string?
    def _build_command(self):
        self._cmdline = self._command
        self._cmdline += ["-X", "READY"]
        assert self.subscriptions, "Missing subscriptions, required for receiver!"
        self._cmdline.append("-a")
        self._cmdline.append(",".join(self.subscriptions))
        if self.receive_count is not None:
            self._cmdline.append("-c")
            self._cmdline.append(str(self.receive_count))
        if self.recv_count is not None:
            self._cmdline.append("-b")
            self._cmdline.append(str(self.recv_count))
        if self.incoming_window is not None:
            self._cmdline.append("-w")
            self._cmdline.append(str(self.incoming_window))
        if self.timeout is not None:
            self._cmdline.append("-t")
            self._cmdline.append(str(self.timeout))
        if self.report_interval is not None:
            self._cmdline.append("-e")
            self._cmdline.append(str(self.report_interval))
        if self.send_reply:
            self._cmdline.append("-R")
        if self.outgoing_window is not None:
            self._cmdline.append("-W")
            self._cmdline.append(str(self.outgoing_window))
        if self.forwards:
            self._cmdline.append("-F")
            self._cmdline.append(",".join(self.forwards))
        if self.name is not None:
            self._cmdline.append("-N")
            self._cmdline.append(str(self.name))

    def _ready(self):
        """ wait for subscriptions to complete setup. """
        r = self._process.stdout.readline()
        assert r == "READY\n", "Unexpected input while waiting for receiver to initialize: %s" % r

class MessengerSenderC(MessengerSender):
    def __init__(self):
        MessengerSender.__init__(self)
        self._command = ["msgr-send"]

class MessengerSenderValgrind(MessengerSenderC):
    """ Run the C sender under Valgrind
    """
    def __init__(self, suppressions=None):
        MessengerSenderC.__init__(self)
        if not suppressions:
            suppressions = os.path.join(os.path.dirname(__file__),
                                        "valgrind.supp" )
        self._command = ["valgrind", "--error-exitcode=1", "--quiet",
                         "--trace-children=yes", "--leak-check=full",
                         "--suppressions=%s" % suppressions] + self._command

class MessengerReceiverC(MessengerReceiver):
    def __init__(self):
        MessengerReceiver.__init__(self)
        self._command = ["msgr-recv"]

class MessengerReceiverValgrind(MessengerReceiverC):
    """ Run the C receiver under Valgrind
    """
    def __init__(self, suppressions=None):
        MessengerReceiverC.__init__(self)
        if not suppressions:
            suppressions = os.path.join(os.path.dirname(__file__),
                                        "valgrind.supp" )
        self._command = ["valgrind", "--error-exitcode=1", "--quiet",
                         "--trace-children=yes", "--leak-check=full",
                         "--suppressions=%s" % suppressions] + self._command

class MessengerSenderPython(MessengerSender):
    def __init__(self):
        MessengerSender.__init__(self)
        self._command = ["msgr-send.py"]


class MessengerReceiverPython(MessengerReceiver):
    def __init__(self):
        MessengerReceiver.__init__(self)
        self._command = ["msgr-recv.py"]


#
# Tests that run the apps
#

class AppTests(common.Test):

    def __init__(self, *args):
        common.Test.__init__(self, *args)
        self.is_valgrind = False

    def default(self, name, value, **kwargs):
        if self.is_valgrind:
            default = kwargs.get("valgrind", value)
        else:
            default = value
        return common.Test.default(self, name, default, **kwargs)

    @property
    def iterations(self):
        return int(self.default("iterations", 2, fast=1, valgrind=1))

    @property
    def send_count(self):
        return int(self.default("send_count", 17, fast=1, valgrind=1))

    @property
    def target_count(self):
        return int(self.default("target_count", 5, fast=1, valgrind=1))

    @property
    def send_batch(self):
        return int(self.default("send_batch", 7, fast=1, valgrind=1))

    @property
    def forward_count(self):
        return int(self.default("forward_count", 5, fast=1, valgrind=1))

    @property
    def port_count(self):
        return int(self.default("port_count", 3, fast=1, valgrind=1))

    @property
    def sender_count(self):
        return int(self.default("sender_count", 3, fast=1, valgrind=1))

    def valgrind_test(self):
        if "VALGRIND" not in os.environ:
            raise common.Skipped("Skipping test - $VALGRIND not set.")
        self.is_valgrind = True

    def setup(self):
        self.senders = []
        self.receivers = []

    def teardown(self):
        pass

    def _do_test(self, iterations=1):
        verbose = self.verbose

        for R in self.receivers:
            R.start( verbose )

        for j in range(iterations):
            for S in self.senders:
                S.start( verbose )

            for S in self.senders:
                S.wait()
                #print("SENDER OUTPUT:")
                #print( S.stdout() )
                assert S.status() == 0, "Command '%s' failed" % str(S.cmdline())

        for R in self.receivers:
            R.wait()
            #print("RECEIVER OUTPUT")
            #print( R.stdout() )
            assert R.status() == 0, "Command '%s' failed" % str(R.cmdline())

#
# Traffic passing tests based on the Messenger apps
#

class MessengerTests(AppTests):

    _timeout = 60

    def __init__(self, *args):
        AppTests.__init__(self, *args)

    def _do_oneway_test(self, receiver, sender):
        """ Send N messages to a receiver.
        Parameters:
        iterations - repeat the senders this many times
        target_count = # of targets to send to.
        send_count = # messages sent to each target
        """
        iterations = self.iterations
        send_count = self.send_count
        target_count = self.target_count

        send_total = send_count * target_count
        receive_total = send_total * iterations

        port = common.free_tcp_ports()[0]

        receiver.subscriptions = ["amqp://~0.0.0.0:%s" % port]
        receiver.receive_count = receive_total
        receiver.timeout = MessengerTests._timeout
        self.receivers.append( receiver )

        sender.targets = ["amqp://0.0.0.0:%s/X%d" % (port, j) for j in range(target_count)]
        sender.send_count = send_total
        sender.timeout = MessengerTests._timeout
        self.senders.append( sender )

        self._do_test(iterations)

    def _do_echo_test(self, receiver, sender):
        """ Send N messages to a receiver, which responds to each.
        Parameters:
        iterations - repeat the senders this many times
        target_count - # targets to send to
        send_count = # messages sent to each target
        send_batch - wait for replies after this many messages sent
        """
        iterations = self.iterations
        send_count = self.send_count
        target_count = self.target_count
        send_batch = self.send_batch

        send_total = send_count * target_count
        receive_total = send_total * iterations

        port = common.free_tcp_ports()[0]

        receiver.subscriptions = ["amqp://~0.0.0.0:%s" % port]
        receiver.receive_count = receive_total
        receiver.send_reply = True
        receiver.timeout = MessengerTests._timeout
        self.receivers.append( receiver )

        sender.targets = ["amqp://0.0.0.0:%s/%dY" % (port, j) for j in range(target_count)]
        sender.send_count = send_total
        sender.get_reply = True
        sender.send_batch = send_batch
        sender.timeout = MessengerTests._timeout
        self.senders.append( sender )

        self._do_test(iterations)

    def _do_relay_test(self, receiver, relay, sender):
        """ Send N messages to a receiver, which replies to each and forwards
        each of them to different receiver.
        Parameters:
        iterations - repeat the senders this many times
        target_count - # targets to send to
        send_count = # messages sent to each target
        send_batch - wait for replies after this many messages sent
        forward_count - forward to this many targets
        """
        iterations = self.iterations
        send_count = self.send_count
        target_count = self.target_count
        send_batch = self.send_batch
        forward_count = self.forward_count

        send_total = send_count * target_count
        receive_total = send_total * iterations

        port = common.free_tcp_ports()[0]

        receiver.subscriptions = ["amqp://~0.0.0.0:%s" % port]
        receiver.receive_count = receive_total
        receiver.send_reply = True
        # forward to 'relay' - uses two links
        # ## THIS FAILS:
        # receiver.forwards = ["amqp://Relay/%d" % j for j in range(forward_count)]
        receiver.forwards = ["amqp://Relay"]
        receiver.timeout = MessengerTests._timeout
        self.receivers.append( receiver )

        relay.subscriptions = ["amqp://0.0.0.0:%s" % port]
        relay.name = "Relay"
        relay.receive_count = receive_total
        relay.timeout = MessengerTests._timeout
        self.receivers.append( relay )

        # send to 'receiver'
        sender.targets = ["amqp://0.0.0.0:%s/X%dY" % (port, j) for j in range(target_count)]
        sender.send_count = send_total
        sender.get_reply = True
        sender.timeout = MessengerTests._timeout
        self.senders.append( sender )

        self._do_test(iterations)


    def _do_star_topology_test(self, r_factory, s_factory):
        """
        A star-like topology, with a central receiver at the hub, and senders at
        the spokes.  Each sender will connect to each of the ports the receiver is
        listening on.  Each sender will then create N links per each connection.
        Each sender will send X messages per link, waiting for a response.
        Parameters:
        iterations - repeat the senders this many times
        port_count - # of ports the receiver will listen on.  Each sender connects
                     to all ports.
        sender_count - # of senders
        target_count - # of targets per connection
        send_count - # of messages sent to each target
        send_batch - # of messages to send before waiting for response
        """
        iterations = self.iterations
        port_count = self.port_count
        sender_count = self.sender_count
        target_count = self.target_count
        send_count = self.send_count
        send_batch = self.send_batch

        send_total = port_count * target_count * send_count
        receive_total = send_total * sender_count * iterations

        ports = common.free_tcp_ports(port_count)

        receiver = r_factory()
        receiver.subscriptions = ["amqp://~0.0.0.0:%s" % port for port in ports]
        receiver.receive_count = receive_total
        receiver.send_reply = True
        receiver.timeout = MessengerTests._timeout
        self.receivers.append( receiver )

        for i in range(sender_count):
            sender = s_factory()
            sender.targets = ["amqp://0.0.0.0:%s/%d" % (port, j) for port in ports for j in range(target_count)]
            sender.send_count = send_total
            sender.send_batch = send_batch
            sender.get_reply = True
            sender.timeout = MessengerTests._timeout
            self.senders.append( sender )

        self._do_test(iterations)

    def test_oneway_C(self):
        self._do_oneway_test(MessengerReceiverC(), MessengerSenderC())

    def test_oneway_valgrind(self):
        self.valgrind_test()
        self._do_oneway_test(MessengerReceiverValgrind(), MessengerSenderValgrind())

    def test_oneway_Python(self):
        self._do_oneway_test(MessengerReceiverPython(), MessengerSenderPython())

    def test_oneway_C_Python(self):
        self._do_oneway_test(MessengerReceiverC(), MessengerSenderPython())

    def test_oneway_Python_C(self):
        self._do_oneway_test(MessengerReceiverPython(), MessengerSenderC())

    def test_echo_C(self):
        self._do_echo_test(MessengerReceiverC(), MessengerSenderC())

    def test_echo_valgrind(self):
        self.valgrind_test()
        self._do_echo_test(MessengerReceiverValgrind(), MessengerSenderValgrind())

    def test_echo_Python(self):
        self._do_echo_test(MessengerReceiverPython(), MessengerSenderPython())

    def test_echo_C_Python(self):
        self._do_echo_test(MessengerReceiverC(), MessengerSenderPython())

    def test_echo_Python_C(self):
        self._do_echo_test(MessengerReceiverPython(), MessengerSenderC())

    def test_relay_C(self):
        self._do_relay_test(MessengerReceiverC(), MessengerReceiverC(), MessengerSenderC())

    def test_relay_valgrind(self):
        self.valgrind_test()
        self._do_relay_test(MessengerReceiverValgrind(), MessengerReceiverValgrind(), MessengerSenderValgrind())

    def test_relay_C_Python(self):
        self._do_relay_test(MessengerReceiverC(), MessengerReceiverPython(), MessengerSenderPython())

    def test_relay_Python(self):
        self._do_relay_test(MessengerReceiverPython(), MessengerReceiverPython(), MessengerSenderPython())

    def test_star_topology_C(self):
        self._do_star_topology_test( MessengerReceiverC, MessengerSenderC )

    def test_star_topology_valgrind(self):
        self.valgrind_test()
        self._do_star_topology_test( MessengerReceiverValgrind, MessengerSenderValgrind )

    def test_star_topology_Python(self):
        self._do_star_topology_test( MessengerReceiverPython, MessengerSenderPython )

    def test_star_topology_Python_C(self):
        self._do_star_topology_test( MessengerReceiverPython, MessengerSenderC )

    def test_star_topology_C_Python(self):
        self._do_star_topology_test( MessengerReceiverPython, MessengerSenderC )
