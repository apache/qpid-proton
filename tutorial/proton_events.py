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
import heapq, os, Queue, re, socket, time, types
from proton import dispatch, generate_uuid, PN_ACCEPTED, SASL, symbol, ulong, Url
from proton import Collector, Connection, Delivery, Described, Endpoint, Event, Link, Terminus, Timeout
from proton import Message, Handler, ProtonException, Transport, TransportException, ConnectionException
from select import select

class AmqpConnection(object):

    def __init__(self, conn, sock, events, heartbeat=None):
        self.events = events
        self.conn = conn
        self.transport = Transport()
        if heartbeat: self.transport.idle_timeout = heartbeat
        self.transport.bind(self.conn)
        self.socket = sock
        self.socket.setblocking(0)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.write_done = False
        self.read_done = False
        self._closed = False

    def accept(self, force_sasl=True):
        if force_sasl:
            sasl = self.transport.sasl()
            sasl.mechanisms("ANONYMOUS")
            sasl.server()
            sasl.done(SASL.OK)
        #TODO: use SASL anyway if requested by peer
        return self

    def connect(self, host, port=None, username=None, password=None, force_sasl=True):
        if username and password:
            sasl = self.transport.sasl()
            sasl.plain(username, password)
        elif force_sasl:
            sasl = self.transport.sasl()
            sasl.mechanisms('ANONYMOUS')
            sasl.client()
        try:
            self.socket.connect_ex((host, port or 5672))
        except socket.gaierror, e:
            raise ConnectionException("Cannot resolve '%s': %s" % (host, e))
        return self

    def _closed_cleanly(self):
        return self.conn.state & Endpoint.LOCAL_CLOSED and self.conn.state & Endpoint.REMOTE_CLOSED

    def closed(self):
        if not self._closed and self.write_done and self.read_done:
            self.close()
            return True
        else:
            return False

    def close(self):
        self.socket.close()
        self._closed = True

    def fileno(self):
        return self.socket.fileno()

    def reading(self):
        if self.read_done: return False
        c = self.transport.capacity()
        if c > 0:
            return True
        elif c < 0:
            self.read_done = True
        return False

    def writing(self):
        if self.write_done: return False
        try:
            p = self.transport.pending()
            if p > 0:
                return True
            elif p < 0:
                self.write_done = True
                return False
            else: # p == 0
                return False
        except TransportException, e:
            self.write_done = True
            return False

    def readable(self):
        c = self.transport.capacity()
        if c > 0:
            try:
                data = self.socket.recv(c)
                if data:
                    self.transport.push(data)
                else:
                    if not self._closed_cleanly():
                        self.read_done = True
                        self.write_done = True
                    else:
                        self.transport.close_tail()
            except TransportException, e:
                print "Error on read: %s" % e
                self.read_done = True
            except socket.error, e:
                print "Error on recv: %s" % e
                self.read_done = True
                self.write_done = True
        elif c < 0:
            self.read_done = True

    def writable(self):
        try:
            p = self.transport.pending()
            if p > 0:
                data = self.transport.peek(p)
                n = self.socket.send(data)
                self.transport.pop(n)
            elif p < 0:
                self.write_done = True
        except TransportException, e:
            print "Error on write: %s" % e
            self.write_done = True
        except socket.error, e:
            print "Error on send: %s" % e
            self.write_done = True

    def removed(self):
        if not self._closed_cleanly():
            self.transport.unbind()
            self.events.dispatch(ApplicationEvent("disconnected", connection=self.conn))

    def tick(self):
        t = self.transport.tick(time.time())
        if t: return t - time.time()
        else: return None

class Acceptor:

    def __init__(self, events, loop, host, port):
        self.events = events
        self.loop = loop
        self.socket = socket.socket()
        self.socket.setblocking(0)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((host, port))
        self.socket.listen(5)
        self.loop.add(self)
        self._closed = False

    def closed(self):
        if self._closed:
            self.socket.close()
            return True
        else:
            return False

    def close(self):
        self._closed = True

    def fileno(self):
        return self.socket.fileno()

    def reading(self):
        return not self._closed

    def writing(self):
        return False

    def readable(self):
        sock, addr = self.socket.accept()
        if sock:
            self.loop.add(AmqpConnection(self.events.connection(), sock, self.events).accept())

    def removed(self): pass
    def tick(self): return None

class EventInjector(object):
    def __init__(self, events):
        self.events = events
        self.queue = Queue.Queue()
        self.pipe = os.pipe()
        self._closed = False

    def trigger(self, event):
        self.queue.put(event)
        os.write(self.pipe[1], "!")

    def closed(self):
        return self._closed and self.queue.empty()

    def close(self):
        self._closed = True

    def fileno(self):
        return self.pipe[0]

    def reading(self):
        return True

    def writing(self):
        return False

    def readable(self):
        os.read(self.pipe[0], 512)
        while not self.queue.empty():
            self.events.dispatch(self.queue.get())

    def removed(self): pass
    def tick(self): return None

def nested_handlers(handlers):
    # currently only allows for a single level of nesting
    nested = []
    for h in handlers:
        nested.append(h)
        if hasattr(h, 'handlers'):
            nested.extend(getattr(h, 'handlers'))
    return nested

def add_nested_handler(handler, nested):
    if hasattr(handler, 'handlers'):
        getattr(handler, 'handlers').append(nested)
    else:
        handler.handlers = [nested]

class Events(object):
    def __init__(self, *handlers):
        self.collector = Collector()
        self.handlers = handlers

    def connection(self):
        conn = Connection()
        conn.collect(self.collector)
        return conn

    def process(self):
        while True:
            ev = self.collector.peek()
            if ev:
                self.dispatch(ev)
                self.collector.pop()
            else:
                return

    def dispatch(self, event):
        for h in self.handlers:
            event.dispatch(h)

    @property
    def next_interval(self):
        return None

    @property
    def empty(self):
        return self.collector.peek() == None

class ExtendedEventType(object):
    def __init__(self, name):
        self.name = name
        self.method = "on_%s" % name

class ApplicationEvent(Event):
    def __init__(self, typename, connection=None, session=None, link=None, delivery=None, subject=None):
        self.type = ExtendedEventType(typename)
        self.subject = subject
        if delivery:
            self.context = delivery
            self.clazz = "pn_delivery"
        elif link:
            self.context = link
            self.clazz = "pn_link"
        elif session:
            self.context = session
            self.clazz = "pn_session"
        elif connection:
            self.context = connection
            self.clazz = "pn_connection"
        else:
            self.context = None
            self.clazz = "none"

    def __repr__(self):
        objects = [self.context, self.subject]
        return "%s(%s)" % (self.type.name,
                           ", ".join([str(o) for o in objects if o is not None]))

class StartEvent(ApplicationEvent):
    def __init__(self, reactor):
        super(StartEvent, self).__init__("start")
        self.reactor = reactor

class ScheduledEvents(Events):
    def __init__(self, *handlers):
        super(ScheduledEvents, self).__init__(*handlers)
        self._events = []

    def schedule(self, deadline, event):
        heapq.heappush(self._events, (deadline, event))

    def process(self):
        super(ScheduledEvents, self).process()
        while self._events and self._events[0][0] <= time.time():
            self.dispatch(heapq.heappop(self._events)[1])

    @property
    def next_interval(self):
        if len(self._events):
            deadline = self._events[0][0]
            now = time.time()
            return deadline - now if deadline > now else 0
        else:
            return None

    @property
    def empty(self):
        return super(ScheduledEvents, self).empty and len(self._events) == 0

def _min(a, b):
    if a and b: return min(a, b)
    elif a: return a
    else: return b

class SelectLoop(object):

    def __init__(self, events):
        self.events = events
        self.selectables = []
        self._abort = False

    def abort(self):
        self._abort = True

    def add(self, selectable):
        self.selectables.append(selectable)

    def remove(self, selectable):
        self.selectables.remove(selectable)

    @property
    def redundant(self):
        return self.events.empty and not self.selectables

    @property
    def aborted(self):
        return self._abort

    def run(self):
        while not (self._abort or self.redundant):
            self.do_work()

    def do_work(self, timeout=None):
        """@return True if some work was done, False if time-out expired"""
        self.events.process()
        if self._abort: return

        stable = False
        while not stable:
            reading = []
            writing = []
            closed = []
            tick = None
            for s in self.selectables:
                if s.reading(): reading.append(s)
                if s.writing(): writing.append(s)
                if s.closed(): closed.append(s)
                else: tick = _min(tick, s.tick())

            for s in closed:
                self.selectables.remove(s)
                s.removed()
            stable = len(closed) == 0

        if self.redundant:
            return

        if timeout and timeout < 0:
            timeout = 0
        if self.events.next_interval and (timeout is None or self.events.next_interval < timeout):
            timeout = self.events.next_interval
        if tick:
            timeout = _min(tick, timeout)
        if reading or writing or timeout:
            readable, writable, _ = select(reading, writing, [], timeout)
            for s in self.selectables:
                s.tick()
            for s in readable:
                s.readable()
            for s in writable:
                s.writable()

            return bool(readable or writable)
        else:
            return False


class Handshaker(Handler):

    def on_connection_remote_open(self, event):
        conn = event.connection
        if conn.state & Endpoint.LOCAL_UNINIT:
            conn.open()

    def on_session_remote_open(self, event):
        ssn = event.session
        if ssn.state & Endpoint.LOCAL_UNINIT:
            ssn.open()

    def on_link_remote_open(self, event):
        link = event.link
        if link.state & Endpoint.LOCAL_UNINIT:
            link.source.copy(link.remote_source)
            link.target.copy(link.remote_target)
            link.open()

    def on_connection_remote_close(self, event):
        conn = event.connection
        if not (conn.state & Endpoint.LOCAL_CLOSED):
            conn.close()

    def on_session_remote_close(self, event):
        ssn = event.session
        if not (ssn.state & Endpoint.LOCAL_CLOSED):
            ssn.close()

    def on_link_remote_close(self, event):
        link = event.link
        if not (link.state & Endpoint.LOCAL_CLOSED):
            link.close()

class FlowController(Handler):

    def __init__(self, window=1):
        self.window = window

    def top_up(self, link):
        delta = self.window - link.credit
        link.flow(delta)

    def on_link_local_open(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_link_remote_open(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_link_flow(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_delivery(self, event):
        if not event.delivery.released and event.delivery.link.is_receiver:
            self.top_up(event.delivery.link)

class ScopedHandler(Handler):

    scopes = {
        "pn_connection": ["connection"],
        "pn_session": ["session", "connection"],
        "pn_link": ["link", "session", "connection"],
        "pn_delivery": ["delivery", "link", "session", "connection"]
    }

    def on_unhandled(self, method, args):
        event = args[0]
        if event.type in [Event.CONNECTION_FINAL, Event.SESSION_FINAL, Event.LINK_FINAL]:
            return
        objects = [getattr(event, attr) for attr in self.scopes.get(event.clazz, [])]
        targets = [getattr(o, "context") for o in objects if hasattr(o, "context")]
        handlers = [getattr(t, event.type.method) for t in nested_handlers(targets) if hasattr(t, event.type.method)]
        for h in handlers:
            h(event)

class OutgoingMessageHandler(Handler):
    def __init__(self, auto_settle=True, delegate=None):
        self.auto_settle = auto_settle
        self.delegate = delegate

    def on_link_flow(self, event):
        if event.link.is_sender and event.link.credit:
            self.on_credit(event)

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.released: return
        if dlv.link.is_sender and dlv.updated:
            if dlv.remote_state == Delivery.ACCEPTED:
                self.on_accepted(event)
            elif dlv.remote_state == Delivery.REJECTED:
                self.on_rejected(event)
            elif dlv.remote_state == Delivery.RELEASED:
                self.on_released(event)
            elif dlv.remote_state == Delivery.MODIFIED:
                self.on_modified(event)
            if dlv.settled:
                self.on_settled(event)
            if self.auto_settle:
                dlv.settle()

    def on_credit(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_credit', event)

    def on_accepted(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_accepted', event)

    def on_rejected(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_rejected', event)

    def on_released(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_released', event)

    def on_modified(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_modified', event)

    def on_settled(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_settled', event)

def recv_msg(delivery):
    msg = Message()
    msg.decode(delivery.link.recv(delivery.pending))
    delivery.link.advance()
    return msg

class Reject(ProtonException):
  """
  An exception that indicate a message should be rejected
  """
  pass

class Acking(object):
    def accept(self, delivery):
        self.settle(delivery, Delivery.ACCEPTED)

    def reject(self, delivery):
        self.settle(delivery, Delivery.REJECTED)

    def release(self, delivery, delivered=True):
        if delivered:
            self.settle(delivery, Delivery.MODIFIED)
        else:
            self.settle(delivery, Delivery.RELEASED)

    def settle(self, delivery, state=None):
        if state:
            delivery.update(state)
        delivery.settle()

class IncomingMessageHandler(Handler, Acking):
    def __init__(self, auto_accept=True, delegate=None):
        self.delegate = delegate
        self.auto_accept = auto_accept

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.released or not dlv.link.is_receiver: return
        if dlv.readable and not dlv.partial:
            event.message = recv_msg(dlv)
            try:
                self.on_message(event)
                if self.auto_accept:
                    dlv.update(Delivery.ACCEPTED)
                    dlv.settle()
            except Reject:
                dlv.update(Delivery.REJECTED)
                dlv.settle()
        elif dlv.updated and dlv.settled:
            self.on_settled(event)

    def on_message(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_message', event)

    def on_settled(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_settled', event)

class EndpointStateHandler(Handler):
    def __init__(self, peer_close_is_error=False, delegate=None):
        self.delegate = delegate
        self.peer_close_is_error = peer_close_is_error

    def is_local_open(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_ACTIVE

    def is_local_uninitialised(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_UNINIT

    def is_local_closed(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_CLOSED

    def is_remote_open(self, endpoint):
        return endpoint.state & Endpoint.REMOTE_ACTIVE

    def is_remote_closed(self, endpoint):
        return endpoint.state & Endpoint.REMOTE_CLOSED

    def print_error(self, endpoint, endpoint_type):
        if endpoint.remote_condition:
            print endpoint.remote_condition.description
        elif self.is_local_open(endpoint) and self.is_remote_closed(endpoint):
            print "%s closed by peer" % endpoint_type

    def on_link_remote_close(self, event):
        if event.link.remote_condition:
            self.on_link_error(event)
        elif self.is_local_closed(event.link):
            self.on_link_closed(event)
        else:
            self.on_link_closing(event)
        event.link.close()

    def on_session_remote_close(self, event):
        if event.session.remote_condition:
            self.on_session_error(event)
        elif self.is_local_closed(event.session):
            self.on_session_closed(event)
        else:
            self.on_session_closing(event)
        event.session.close()

    def on_connection_remote_close(self, event):
        if event.connection.remote_condition:
            self.on_connection_error(event)
        elif self.is_local_closed(event.connection):
            self.on_connection_closed(event)
        else:
            self.on_connection_closing(event)
        event.connection.close()

    def on_connection_local_open(self, event):
        if self.is_remote_open(event.connection):
            self.on_connection_opened(event)

    def on_connection_remote_open(self, event):
        if self.is_local_open(event.connection):
            self.on_connection_opened(event)
        elif self.is_local_uninitialised(event.connection):
            self.on_connection_opening(event)
            event.connection.open()

    def on_session_local_open(self, event):
        if self.is_remote_open(event.session):
            self.on_session_opened(event)

    def on_session_remote_open(self, event):
        if self.is_local_open(event.session):
            self.on_session_opened(event)
        elif self.is_local_uninitialised(event.session):
            self.on_session_opening(event)
            event.session.open()

    def on_link_local_open(self, event):
        if self.is_remote_open(event.link):
            self.on_link_opened(event)

    def on_link_remote_open(self, event):
        if self.is_local_open(event.link):
            self.on_link_opened(event)
        elif self.is_local_uninitialised(event.link):
            self.on_link_opening(event)
            event.link.open()

    def on_connection_opened(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_opened', event)

    def on_session_opened(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_opened', event)

    def on_link_opened(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_opened', event)

    def on_connection_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_opening', event)

    def on_session_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_opening', event)

    def on_link_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_opening', event)

    def on_connection_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_error', event)
        else:
            self.print_error(event.connection, "connection")

    def on_session_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_error', event)
        else:
            self.print_error(event.session, "session")
            event.connection.close()

    def on_link_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_error', event)
        else:
            self.print_error(event.link, "link")
            event.connection.close()

    def on_connection_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_closed', event)

    def on_session_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_closed', event)

    def on_link_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_closed', event)

    def on_connection_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_closing', event)
        elif self.peer_close_is_error:
            self.on_connection_error(event)

    def on_session_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_closing', event)
        elif self.peer_close_is_error:
            self.on_session_error(event)

    def on_link_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_closing', event)
        elif self.peer_close_is_error:
            self.on_link_error(event)

class MessagingHandler(Handler, Acking):
    def __init__(self, prefetch=10, auto_accept=True, auto_settle=True, peer_close_is_error=False):
        self.handlers = []
        # FlowController if used needs to see event before
        # IncomingMessageHandler, as the latter may involve the
        # delivery being released
        if prefetch:
            self.handlers.append(FlowController(prefetch))
        self.handlers.append(EndpointStateHandler(peer_close_is_error, self))
        self.handlers.append(IncomingMessageHandler(auto_accept, self))
        self.handlers.append(OutgoingMessageHandler(auto_settle, self))

def delivery_tags():
    count = 1
    while True:
        yield str(count)
        count += 1

def send_msg(sender, msg, tag=None, handler=None, transaction=None):
    dlv = sender.delivery(tag or next(sender.tags))
    if transaction:
        dlv.local.data = [transaction.id]
        dlv.update(0x34)
    if handler:
        dlv.context = handler
    sender.send(msg.encode())
    sender.advance()
    return dlv

def _send_msg(self, msg, tag=None, handler=None, transaction=None):
    return send_msg(self, msg, tag, handler, transaction)

class TransactionalAcking(object):
    def accept(self, delivery, transaction):
        self.settle(delivery, transaction, PN_ACCEPTED)

    def settle(self, delivery, transaction, state=None):
        if state:
            delivery.local.data = [transaction.id, Described(ulong(state), [])]
            delivery.update(0x34)
        delivery.settle()

class TransactionHandler(OutgoingMessageHandler, TransactionalAcking):
    def __init__(self, auto_settle=True, delegate=None):
        super(TransactionHandler, self).__init__(auto_settle, delegate)

    def on_settled(self, event):
        if hasattr(event.delivery, "transaction"):
            event.transaction = event.delivery.transaction
            event.delivery.transaction.handle_outcome(event)

    def on_transaction_declared(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_declared', event)

    def on_transaction_committed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_committed', event)

    def on_transaction_aborted(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_aborted', event)

    def on_transaction_declare_failed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_declare_failed', event)

    def on_transaction_commit_failed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_commit_failed', event)

class TransactionalClientHandler(Handler, TransactionalAcking):
    def __init__(self, prefetch=10, auto_accept=False, auto_settle=True, peer_close_is_error=False):
        super(TransactionalClientHandler, self).__init__()
        self.handlers = []
        # FlowController if used needs to see event before
        # IncomingMessageHandler, as the latter may involve the
        # delivery being released
        if prefetch:
            self.handlers.append(FlowController(prefetch))
        self.handlers.append(EndpointStateHandler(peer_close_is_error, self))
        self.handlers.append(IncomingMessageHandler(auto_accept, self))
        self.handlers.append(TransactionHandler(auto_settle, self))


class Transaction(object):
    def __init__(self, txn_ctrl, handler):
        self.txn_ctrl = txn_ctrl
        self.handler = handler
        self.id = None
        self._declare = None
        self._discharge = None
        self.failed = False
        self.declare()

    def commit(self):
        self.discharge(False)

    def abort(self):
        self.discharge(True)

    def declare(self):
        self._declare = self._send_ctrl(symbol(u'amqp:declare:list'), [None])

    def discharge(self, failed):
        self.failed = failed
        self._discharge = self._send_ctrl(symbol(u'amqp:discharge:list'), [self.id, failed])

    def _send_ctrl(self, descriptor, value):
        delivery = self.txn_ctrl.send_msg(Message(body=Described(descriptor, value)), handler=self.handler)
        delivery.transaction = self
        return delivery

    def handle_outcome(self, event):
        if event.delivery == self._declare:
            if event.delivery.remote.data:
                self.id = event.delivery.remote.data[0]
                self.handler.on_transaction_declared(event)
            elif event.delivery.remote_state == Delivery.REJECTED:
                self.handler.on_transaction_declare_failed(event)
            else:
                print "Unexpected outcome for declare: %s" % event.delivery.remote_state
                self.handler.on_transaction_declare_failed(event)
        elif event.delivery == self._discharge:
            if event.delivery.remote_state == Delivery.REJECTED:
                if not self.failed:
                    self.handler.on_transaction_commit_failed(event)
            else:
                if self.failed:
                    self.handler.on_transaction_aborted(event)
                else:
                    self.handler.on_transaction_committed(event)


class LinkOption(object):
    def apply(self, link): pass
    def test(self, link): return True

class AtMostOnce(LinkOption):
    def apply(self, link):
        link.snd_settle_mode = Link.SND_SETTLED

class AtLeastOnce(LinkOption):
    def apply(self, link):
        link.snd_settle_mode = Link.SND_UNSETTLED
        link.rcv_settle_mode = Link.RCV_FIRST

class SenderOption(LinkOption):
    def apply(self, sender): pass
    def test(self, link): return link.is_sender

class ReceiverOption(LinkOption):
    def apply(self, receiver): pass
    def test(self, link): return link.is_receiver

class Filter(ReceiverOption):
    def __init__(self, filter_set={}):
        self.filter_set = filter_set

    def apply(self, receiver):
        receiver.source.filter.put_dict(self.filter_set)

class Selector(Filter):
    def __init__(self, value, name='selector'):
        super(Selector, self).__init__({symbol(name): Described(symbol('apache.org:selector-filter:string'), value)})

def _apply_link_options(options, link):
    if options:
        if isinstance(options, list):
            for o in options:
                if o.test(link): o.apply(link)
        else:
            if options.test(link): options.apply(link)


class MessagingContext(object):
    def __init__(self, conn, handler=None, ssn=None):
        self.conn = conn
        if handler:
            self.conn.context = handler
        self.conn._mc = self
        self.ssn = ssn
        self.txn_ctrl = None

    def _get_handler(self):
        return self.conn.context

    def _set_handler(self, value):
        self.conn.context = value

    handler = property(_get_handler, _set_handler)

    def create_sender(self, target, source=None, name=None, handler=None, tags=None, options=None):
        snd = self._get_ssn().sender(name or self._get_id(target, source))
        if source:
            snd.source.address = source
        if target:
            snd.target.address = target
        if handler:
            snd.context = handler
        snd.tags = tags or delivery_tags()
        snd.send_msg = types.MethodType(_send_msg, snd)
        _apply_link_options(options, snd)
        snd.open()
        return snd

    def create_receiver(self, source, target=None, name=None, dynamic=False, handler=None, options=None):
        rcv = self._get_ssn().receiver(name or self._get_id(source, target))
        if source:
            rcv.source.address = source
        if dynamic:
            rcv.source.dynamic = True
        if target:
            rcv.target.address = target
        if handler:
            rcv.context = handler
        _apply_link_options(options, rcv)
        rcv.open()
        return rcv

    def create_session(self):
        return MessageContext(conn=None, ssn=self._new_ssn())

    def declare_transaction(self, handler=None):
        if not self.txn_ctrl:
            self.txn_ctrl = self.create_sender(None, name="txn-ctrl")
            self.txn_ctrl.target.type = Terminus.COORDINATOR
            self.txn_ctrl.target.capabilities.put_object(symbol(u'amqp:local-transactions'))
        return Transaction(self.txn_ctrl, handler)

    def close(self):
        if self.ssn:
            self.ssn.close()
        if self.conn:
            self.conn.close()

    def _get_id(self, remote, local):
        if local and remote: "%s-%s-%s" % (self.conn.container, remote, local)
        elif local: return "%s-%s" % (self.conn.container, local)
        elif remote: return "%s-%s" % (self.conn.container, remote)
        else: return "%s-%s" % (self.conn.container, str(generate_uuid()))

    def _get_ssn(self):
        if not self.ssn:
            self.ssn = self._new_ssn()
            self.ssn.context = self
        return self.ssn

    def _new_ssn(self):
        ssn = self.conn.session()
        ssn.open()
        return ssn

    def on_session_remote_close(self, event):
        if self.conn:
            self.conn.close()

class Connector(Handler):
    def attach_to(self, loop):
        self.loop = loop

    def _connect(self, connection):
        host, port = connection.address.next()
        #print "connecting to %s:%i" % (host, port)
        heartbeat = connection.heartbeat if hasattr(connection, 'heartbeat') else None
        self.loop.add(AmqpConnection(connection, socket.socket(), self.loop.events, heartbeat=heartbeat).connect(host, port))

    def on_connection_local_open(self, event):
        if hasattr(event.connection, "address"):
            self._connect(event.connection)

    def on_connection_remote_open(self, event):
        if hasattr(event.connection, "reconnect"):
            event.connection.reconnect.reset()

    def on_disconnected(self, event):
        if hasattr(event.connection, "reconnect"):
            delay = event.connection.reconnect.next()
            if delay == 0:
                print "Disconnected, reconnecting..."
                self._connect(event.connection)
            else:
                print "Disconnected will try to reconnect after %s seconds" % delay
                self.loop.schedule(time.time() + delay, connection=event.connection, subject=self)
        else:
            print "Disconnected"

    def on_timer(self, event):
        if event.subject == self and event.connection:
            self._connect(event.connection)

class Backoff(object):
    def __init__(self):
        self.delay = 0

    def reset(self):
        self.delay = 0

    def next(self):
        current = self.delay
        if current == 0:
            self.delay = 0.1
        else:
            self.delay = min(10, 2*current)
        return current

class Urls(object):
    def __init__(self, values):
        self.values = [Url(v) for v in values]
        self.i = iter(self.values)

    def __iter__(self):
        return self

    def _as_pair(self, url):
        return (url.host, url.port)

    def next(self):
        try:
            return self._as_pair(self.i.next())
        except StopIteration:
            self.i = iter(self.values)
            return self._as_pair(self.i.next())

class EventLoop(object):
    def __init__(self, *handlers):
        self.connector = Connector()
        h = [self.connector, ScopedHandler()]
        h.extend(nested_handlers(handlers))
        self.events = ScheduledEvents(*h)
        self.loop = SelectLoop(self.events)
        self.connector.attach_to(self)
        self.trigger = None
        self.container_id = str(generate_uuid())

    def connect(self, url=None, urls=None, address=None, handler=None, reconnect=None, heartbeat=None):
        context = MessagingContext(self.events.connection(), handler=handler)
        context.conn.container = self.container_id or str(generate_uuid())
        context.conn.heartbeat = heartbeat
        if url: context.conn.address = Urls([url])
        elif urls: context.conn.address = Urls(urls)
        elif address: context.conn.address = address
        else: raise ValueError("One of url, urls or address required")
        if reconnect:
            context.conn.reconnect = reconnect
        elif reconnect is None:
            context.conn.reconnect = Backoff()
        context.conn.open()
        return context

    def listen(self, url):
        host, port = Url(url).next()
        return Acceptor(self.events, self, host, port)

    def schedule(self, deadline, connection=None, session=None, link=None, delivery=None, subject=None):
        self.events.schedule(deadline, ApplicationEvent("timer", connection, session, link, delivery, subject))

    def get_event_trigger(self):
        if not self.trigger or self.trigger.closed():
            self.trigger = EventInjector(self.events)
            self.add(self.trigger)
        return self.trigger

    def add(self, selectable):
        self.loop.add(selectable)

    def remove(self, selectable):
        self.loop.remove(selectable)

    def run(self):
        self.events.dispatch(StartEvent(self))
        self.loop.run()

    def stop(self):
        self.loop.abort()

    def do_work(self, timeout=None):
        return self.loop.do_work(timeout)

EventLoop.DEFAULT = EventLoop()

def connect(url=None, urls=None, address=None, handler=None, reconnect=None, eventloop=None, heartbeat=None):
    if not eventloop:
        eventloop = EventLoop.DEFAULT
    return eventloop.connect(url=url, urls=urls, address=address, handler=handler, reconnect=reconnect, heartbeat=heartbeat)

def run(eventloop=None):
    if not eventloop:
        eventloop = EventLoop.DEFAULT
    eventloop.run()

class BlockingLink(object):
    def __init__(self, connection, link):
        self.connection = connection
        self.link = link
        self.connection.wait(lambda: not (self.link.state & Endpoint.REMOTE_UNINIT),
                             msg="Opening link %s" % link.name)

    def close(self):
        self.connection.wait(not (self.link.state & Endpoint.REMOTE_ACTIVE),
                             msg="Closing link %s" % link.name)

    # Access to other link attributes.
    def __getattr__(self, name): return getattr(self.link, name)

class BlockingSender(BlockingLink):
    def __init__(self, connection, sender):
        super(BlockingSender, self).__init__(connection, sender)

    def send_msg(self, msg):
        delivery = send_msg(self.link, msg)
        self.connection.wait(lambda: delivery.settled, msg="Sending on sender %s" % self.link.name)

class BlockingReceiver(BlockingLink):
    def __init__(self, connection, receiver, credit=1):
        super(BlockingReceiver, self).__init__(connection, receiver)
        if credit: receiver.flow(credit)

class BlockingConnection(Handler):
    def __init__(self, url, timeout=None):
        self.timeout = timeout
        self.events = Events(ScopedHandler())
        self.loop = SelectLoop(self.events)
        self.context = MessagingContext(self.loop.events.connection(), handler=self)
        if isinstance(url, basestring):
            self.url = Url(url)
        else:
            self.url = url
        self.loop.add(
            AmqpConnection(self.context.conn, socket.socket(), self.events).connect(self.url.host, self.url.port))
        self.context.conn.open()
        self.wait(lambda: not (self.context.conn.state & Endpoint.REMOTE_UNINIT),
                  msg="Opening connection")

    def create_sender(self, address, handler=None):
        return BlockingSender(self, self.context.create_sender(address, handler=handler))

    def create_receiver(self, address, credit=1, dynamic=False, handler=None):
        return BlockingReceiver(
            self, self.context.create_receiver(address, dynamic=dynamic, handler=handler), credit=credit)

    def close(self):
        self.context.conn.close()
        self.wait(lambda: not (self.context.conn.state & Endpoint.REMOTE_ACTIVE),
                  msg="Closing connection")

    def run(self):
        """ Hand control over to the event loop (e.g. if waiting indefinitely for incoming messages) """
        self.loop.run()

    def wait(self, condition, timeout=False, msg=None):
        """Call do_work until condition() is true"""
        if timeout is False:
            timeout = self.timeout
        if timeout is None:
            while not condition():
                self.loop.do_work()
        else:
            deadline = time.time() + timeout
            while not condition():
                if not self.loop.do_work(deadline - time.time()):
                    txt = "Connection %s timed out" % self.url
                    if msg: txt += ": " + msg
                    raise Timeout(txt)

    def on_link_remote_close(self, event):
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            self.closed(event.link.remote_condition)

    def on_connection_remote_close(self, event):
        if event.connection.state & Endpoint.LOCAL_ACTIVE:
            self.closed(event.connection.remote_condition)

    def on_disconnected(self, event):
        raise ConnectionException("Connection %s disconnected" % self.url);

    def closed(self, error=None):
        txt = "Connection %s closed" % self.url
        if error:
            txt += " due to: %s" % error
        else:
            txt += " by peer"
        raise ConnectionException(txt)
