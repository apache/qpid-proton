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
from proton import Driver, Connection, SASL, Endpoint, Delivery

class Test:

  def __init__(self, name):
    self.name = name

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

