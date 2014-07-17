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

from random import randint
from threading import Thread
from socket import socket, AF_INET, SOCK_STREAM
from subprocess import Popen,PIPE,STDOUT
import sys, os, string
from proton import Driver, Connection, Transport, SASL, Endpoint, Delivery, \
    SSLDomain, SSLUnavailable


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
    bytes = src.peek(min(c, buffer_size))
    dst.push(bytes)
    src.pop(len(bytes))

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

def isSSLPresent():
    """ True if a suitable SSL library is available.
    """
    try:
        xxx = SSLDomain(SSLDomain.MODE_CLIENT)
        return True
    except SSLUnavailable, e:
        # SSL libraries not installed
        return False

class Test(object):

  def __init__(self, name):
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

class Skipped(Exception):
  skipped = True


class TestServer(object):
  """ Base class for creating test-specific message servers.
  """
  def __init__(self, **kwargs):
    self.args = kwargs
    self.driver = Driver()
    self.host = "127.0.0.1"
    self.port = 0
    if "host" in kwargs:
      self.host = kwargs["host"]
    if "port" in kwargs:
      self.port = kwargs["port"]
    self.driver_timeout = -1
    self.credit_batch = 10
    self.thread = Thread(name="server-thread", target=self.run)
    self.thread.daemon = True
    self.running = True

  def start(self):
    retry = 0
    if self.port == 0:
      self.port = str(randint(49152, 65535))
      retry = 10
    self.listener = self.driver.listener(self.host, self.port)
    while not self.listener and retry > 0:
      retry -= 1
      self.port = str(randint(49152, 65535))
      self.listener = self.driver.listener(self.host, self.port)
    assert self.listener, "No free port for server to listen on!"
    self.thread.start()

  def stop(self):
    self.running = False
    self.driver.wakeup()
    self.thread.join()
    if self.listener:
      self.listener.close()
    cxtr = self.driver.head_connector()
    while cxtr:
      if not cxtr.closed:
        cxtr.close()
      cxtr = cxtr.next()

  # Note: all following methods all run under the thread:

  def run(self):
    while self.running:
      self.driver.wait(self.driver_timeout)
      self.process_listeners()
      self.process_connectors()


  def process_listeners(self):
    """ Service each pending listener
    """
    l = self.driver.pending_listener()
    while l:
      cxtr = l.accept()
      assert(cxtr)
      self.init_connector(cxtr)
      l = self.driver.pending_listener()

  def init_connector(self, cxtr):
    """ Initialize a newly accepted connector
    """
    sasl = cxtr.sasl()
    sasl.mechanisms("ANONYMOUS")
    sasl.server()
    cxtr.connection = Connection()
    if "idle_timeout" in self.args:
      cxtr.transport.idle_timeout = self.args["idle_timeout"]

  def process_connectors(self):
    """ Service each pending connector
    """
    cxtr = self.driver.pending_connector()
    while cxtr:
      self.process_connector(cxtr)
      cxtr = self.driver.pending_connector()

  def process_connector(self, cxtr):
    """ Process a pending connector
    """
    if not cxtr.closed:
      cxtr.process()
      sasl = cxtr.sasl()
      if sasl.state != SASL.STATE_PASS:
        self.authenticate_connector(cxtr)
      else:
        conn = cxtr.connection
        if conn:
          self.service_connection(conn)
      cxtr.process()

  def authenticate_connector(self, cxtr):
    """ Deal with a connector that has not passed SASL
    """
    # by default, just permit anyone
    sasl = cxtr.sasl()
    if sasl.state == SASL.STATE_STEP:
      sasl.done(SASL.OK)

  def service_connection(self, conn):
    """ Process a Connection
    """
    if conn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT:
      conn.open()

    # open all pending sessions
    ssn = conn.session_head(Endpoint.LOCAL_UNINIT)
    while ssn:
      self.init_session(ssn)
      ssn.open()
      ssn = ssn.next(Endpoint.LOCAL_UNINIT)

    # configure and open any pending links
    link = conn.link_head(Endpoint.LOCAL_UNINIT)
    while link:
      self.init_link(link)
      link.open()
      link = link.next(Endpoint.LOCAL_UNINIT);

    ## Step 2: Now drain all the pending deliveries from the connection's
    ## work queue and process them

    delivery = conn.work_head
    while delivery:
      self.process_delivery(delivery)
      delivery = conn.work_head

    ## Step 3: Clean up any links or sessions that have been closed by the
    ## remote.  If the connection has been closed remotely, clean that up
    ## also.

    # teardown any terminating links
    link = conn.link_head(Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED)
    while link:
      link.close()
      link = link.next(Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED)

    # teardown any terminating sessions
    ssn = conn.session_head(Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED)
    while ssn:
      ssn.close(ssn)
      ssn = ssn.next(Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED)

    if conn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED:
      conn.close()

  def init_session(self, ssn):
    """ Test-specific Session initialization
    """
    pass

  def init_link(self, link):
    """ Test-specific Link initialization
    """
    pass

  def process_delivery(self, delivery):
    """ Test-specific Delivery processing.
    """
    pass


class TestServerDrain(TestServer):
  """ A primitive test server that accepts connections and simply discards any
  messages sent to it.
  """
  def __init__(self, **kwargs):
    TestServer.__init__(self, **kwargs)

  def init_link(self, link):
    """ Test-specific Link initialization
    """
    if link.is_receiver:
      link.flow(self.credit_batch)

  def process_delivery(self, delivery):
    """ Just drop any incomming messages
    """
    link = delivery.link
    if delivery.readable:   # inbound data available
      m = link.recv(1024)
      while m:
        #print("Dropping msg...%s" % str(m))
        m = link.recv(1024)
      delivery.update(Delivery.ACCEPTED)
      delivery.settle()
    else:
      link.advance()
    if link.credit == 0:
      link.flow(self.credit_batch)


#
# Classes that wrap the messenger applications msgr-send and msgr-recv.
# These applications reside in the tests/tools/apps directory
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

    def findfile(self, filename, searchpath):
        """Find filename in the searchpath
            return absolute path to the file or None
        """
        paths = string.split(searchpath, os.pathsep)
        for path in paths:
            if os.path.exists(os.path.join(path, filename)):
                return os.path.abspath(os.path.join(path, filename))
        return None

    def start(self, verbose=False):
        """ Begin executing the test """
        cmd = self.cmdline()
        self._verbose = verbose
        if self._verbose:
            print("COMMAND='%s'" % str(cmd))
        #print("ENV='%s'" % str(os.environ.copy()))
        try:
            if os.name=="nt":
                # Windows handles python launch by replacing script 'filename' with
                # 'python abspath-to-filename' in cmdline arg list.
                if cmd[0].endswith('.py'):
                    foundfile = self.findfile(cmd[0], os.environ['PATH'])
                    if foundfile is None:
                        foundfile = self.findfile(cmd[0], os.environ['PYTHONPATH'])
                        assert foundfile is not None, "Unable to locate file '%s' in PATH or PYTHONPATH" % cmd[0]
                    del cmd[0:1]
                    cmd.insert(0, foundfile)
                    cmd.insert(0, sys.executable)
            self._process = Popen(cmd, stdout=PIPE, stderr=STDOUT, bufsize=4096)
        except OSError, e:
            print("ERROR: '%s'" % e)
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
        self._cmdline = self._command
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
        assert r == "READY" + os.linesep, "Unexpected input while waiting for receiver to initialize: %s" % r

class MessengerSenderC(MessengerSender):
    def __init__(self):
        MessengerSender.__init__(self)
        self._command = ["msgr-send"]

class MessengerSenderValgrind(MessengerSenderC):
    """ Run the C sender under Valgrind
    """
    def __init__(self, suppressions=None):
        if "VALGRIND" not in os.environ:
            raise Skipped("Skipping test - $VALGRIND not set.")
        MessengerSenderC.__init__(self)
        if not suppressions:
            suppressions = os.path.join(os.path.dirname(__file__),
                                        "valgrind.supp" )
        self._command = [os.environ["VALGRIND"], "--error-exitcode=1", "--quiet",
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
        if "VALGRIND" not in os.environ:
            raise Skipped("Skipping test - $VALGRIND not set.")
        MessengerReceiverC.__init__(self)
        if not suppressions:
            suppressions = os.path.join(os.path.dirname(__file__),
                                        "valgrind.supp" )
        self._command = [os.environ["VALGRIND"], "--error-exitcode=1", "--quiet",
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



