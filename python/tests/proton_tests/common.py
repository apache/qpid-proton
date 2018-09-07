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

from __future__ import absolute_import

from unittest import TestCase
try:
  from unittest import SkipTest
except:
  try:
    from unittest2 import SkipTest
  except:
    class SkipTest(Exception):
      pass

import sys, os, subprocess
from random import randint
from threading import Thread
from socket import socket, AF_INET, SOCK_STREAM
from subprocess import Popen,PIPE,STDOUT
from string import Template

from proton import SASL, SSL
from proton.reactor import Container
from proton.handlers import Handshaker, FlowController


def free_tcp_ports(count=1):
  """ return a list of 'count' TCP ports that are free to used (ie. unbound)
  """
  retry = 0
  ports = []
  sockets = []
  while len(ports) != count:
    port = randint(49152, 65535)
    sockets.append( socket( AF_INET, SOCK_STREAM ) )
    try:
      sockets[-1].bind( ("0.0.0.0", port ) )
      ports.append( port )
      retry = 0
    except:
      retry += 1
      assert retry != 100, "No free sockets available for test!"
  for s in sockets:
    s.close()
  return ports

def free_tcp_port():
  return free_tcp_ports(1)[0]

def pump_uni(src, dst, buffer_size=1024):
  p = src.pending()
  c = dst.capacity()

  if c < 0:
    if p < 0:
      return False
    else:
      src.close_head()
      return True

  if p < 0:
    dst.close_tail()
  elif p == 0 or c == 0:
    return False
  else:
    binary = src.peek(min(c, buffer_size))
    dst.push(binary)
    src.pop(len(binary))

  return True

def pump(transport1, transport2, buffer_size=1024):
  """ Transfer all pending bytes between two Proton engines
      by repeatedly calling peek/pop and push.
      Asserts that each engine accepts some bytes every time
      (unless it's already closed).
  """
  while (pump_uni(transport1, transport2, buffer_size) or
         pump_uni(transport2, transport1, buffer_size)):
    pass

def findfileinpath(filename, searchpath):
    """Find filename in the searchpath
        return absolute path to the file or None
    """
    paths = searchpath.split(os.pathsep)
    for path in paths:
        if os.path.exists(os.path.join(path, filename)):
            return os.path.abspath(os.path.join(path, filename))
    return None

def isSSLPresent():
    return SSL.present()

createdSASLDb = False

def _cyrusSetup(conf_dir):
  """Write out simple SASL config.
  """
  saslpasswd = ""
  if 'SASLPASSWD' in os.environ:
    saslpasswd = os.environ['SASLPASSWD']
  else:
    saslpasswd = findfileinpath('saslpasswd2', os.getenv('PATH')) or ""
  if os.path.exists(saslpasswd):
    t = Template("""sasldb_path: ${db}
mech_list: EXTERNAL DIGEST-MD5 SCRAM-SHA-1 CRAM-MD5 PLAIN ANONYMOUS
""")
    abs_conf_dir = os.path.abspath(conf_dir)
    subprocess.call(args=['rm','-rf',abs_conf_dir])
    os.mkdir(abs_conf_dir)
    db = os.path.join(abs_conf_dir,'proton.sasldb')
    conf = os.path.join(abs_conf_dir,'proton-server.conf')
    f = open(conf, 'w')
    f.write(t.substitute(db=db))
    f.close()

    cmd_template = Template("echo password | ${saslpasswd} -c -p -f ${db} -u proton user")
    cmd = cmd_template.substitute(db=db, saslpasswd=saslpasswd)
    subprocess.call(args=cmd, shell=True)

    os.environ['PN_SASL_CONFIG_PATH'] = abs_conf_dir
    global createdSASLDb
    createdSASLDb = True

# Globally initialize Cyrus SASL configuration
if SASL.extended():
  _cyrusSetup('sasl-conf')

def ensureCanTestExtendedSASL():
  if not SASL.extended():
    raise Skipped('Extended SASL not supported')
  if not createdSASLDb:
    raise Skipped("Can't Test Extended SASL: Couldn't create auth db")

class DefaultConfig:
    defines = {}

class Test(TestCase):
  config = DefaultConfig()

  def __init__(self, name):
    super(Test, self).__init__(name)
    self.name = name

  def configure(self, config):
    self.config = config

  def default(self, name, value, **profiles):
    default = value
    profile = self.config.defines.get("profile")
    if profile:
      default = profiles.get(profile, default)
    return self.config.defines.get(name, default)

  @property
  def delay(self):
    return float(self.default("delay", "1", fast="0.1"))

  @property
  def timeout(self):
    return float(self.default("timeout", "60", fast="10"))

  @property
  def verbose(self):
    return int(self.default("verbose", 0))


class Skipped(SkipTest):
  skipped = True


class TestServer(object):
  """ Base class for creating test-specific message servers.
  """
  def __init__(self, **kwargs):
    self.args = kwargs
    self.reactor = Container(self)
    self.host = "127.0.0.1"
    self.port = 0
    if "host" in kwargs:
      self.host = kwargs["host"]
    if "port" in kwargs:
      self.port = kwargs["port"]
    self.handlers = [FlowController(10), Handshaker()]
    self.thread = Thread(name="server-thread", target=self.run)
    self.thread.daemon = True
    self.running = True
    self.conditions = []

  def start(self):
    self.reactor.start()
    retry = 0
    if self.port == 0:
      self.port = str(randint(49152, 65535))
      retry = 10
    while retry > 0:
      try:
        self.acceptor = self.reactor.acceptor(self.host, self.port)
        break
      except IOError:
        self.port = str(randint(49152, 65535))
        retry -= 1
    assert retry > 0, "No free port for server to listen on!"
    self.thread.start()

  def stop(self):
    self.running = False
    self.reactor.wakeup()
    self.thread.join()

  # Note: all following methods all run under the thread:

  def run(self):
    self.reactor.timeout = 3.14159265359
    while self.reactor.process():
      if not self.running:
        self.acceptor.close()
        self.reactor.stop()
        break

  def on_connection_bound(self, event):
    if "idle_timeout" in self.args:
      event.transport.idle_timeout = self.args["idle_timeout"]

  def on_connection_local_close(self, event):
    self.conditions.append(event.connection.condition)

  def on_delivery(self, event):
    event.delivery.settle()

#
# Classes that wrap the messenger applications msgr-send and msgr-recv.
# These applications reside in the c/tools directory
#

class MessengerApp(object):
    """ Interface to control a MessengerApp """
    def __init__(self):
        self._cmdline = None
        # options common to Receivers and Senders:
        self.ca_db = None
        self.certificate = None
        self.privatekey = None
        self.password = None
        self._output = None

    def start(self, verbose=False):
        """ Begin executing the test """
        cmd = self.cmdline()
        self._verbose = verbose
        if self._verbose:
            print("COMMAND='%s'" % str(cmd))
        #print("ENV='%s'" % str(os.environ.copy()))
        try:
            # Handle python launch by replacing script 'filename' with
            # 'python abspath-to-filename' in cmdline arg list.
            if cmd[0].endswith('.py'):
                foundfile = findfileinpath(cmd[0], os.getenv('PATH'))
                if foundfile is None:
                    msg = "Unable to locate file '%s' in PATH" % cmd[0]
                    raise Skipped("Skipping test - %s" % msg)

                del cmd[0:1]
                cmd.insert(0, foundfile)
                cmd.insert(0, sys.executable)
            self._process = Popen(cmd, stdout=PIPE,
                                  bufsize=4096, universal_newlines=True)
        except OSError:
            e = sys.exc_info()[1]
            print("ERROR: '%s'" % e)
            msg = "Unable to execute command '%s', is it in your PATH?" % cmd[0]

            # NOTE(flaper87): Skip the test if the command is not found.
            if e.errno == 2:
              raise Skipped("Skipping test - %s" % msg)
            assert False, msg

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
        if not self._output or not self._output[0]:
            return "*** NO STDOUT ***"
        return self._output[0]

    def stderr(self):
        if not self._output or not self._output[1]:
            return "*** NO STDERR ***"
        return self._output[1]

    def cmdline(self):
        if not self._cmdline:
            self._build_command()
        return self._cmdline

    def _build_command(self):
        assert False, "_build_command() needs override"

    def _ready(self):
        assert False, "_ready() needs override"

    def _do_common_options(self):
        """ Common option handling """
        if self.ca_db is not None:
            self._cmdline.append("-T")
            self._cmdline.append(str(self.ca_db))
        if self.certificate is not None:
            self._cmdline.append("-C")
            self._cmdline.append(str(self.certificate))
        if self.privatekey is not None:
            self._cmdline.append("-K")
            self._cmdline.append(str(self.privatekey))
        if self.password is not None:
            self._cmdline.append("-P")
            self._cmdline.append("pass:" + str(self.password))


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
        self._do_common_options()
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
        self._cmdline = os.environ.get("TEST_EXE_PREFIX", "").split()
        self._cmdline += self._command
        self._do_common_options()
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
        assert r.strip() == "READY", "Unexpected input while waiting for receiver to initialize: %s" % r

class MessengerSenderC(MessengerSender):
    def __init__(self):
        MessengerSender.__init__(self)
        self._command = ["msgr-send"]

class MessengerReceiverC(MessengerReceiver):
    def __init__(self):
        MessengerReceiver.__init__(self)
        self._command = ["msgr-recv"]

class ReactorSenderC(MessengerSender):
    def __init__(self):
        MessengerSender.__init__(self)
        self._command = ["reactor-send"]

class ReactorReceiverC(MessengerReceiver):
    def __init__(self):
        MessengerReceiver.__init__(self)
        self._command = ["reactor-recv"]
