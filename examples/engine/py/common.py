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
import errno, os, random, select, time, traceback
from proton import *
from socket import *
from threading import Thread
from heapq import heappush, heappop, nsmallest

class Selectable:

    def __init__(self, transport, socket):
        self.transport = transport
        self.socket = socket
        self.write_done = False
        self.read_done = False

    def closed(self):
        if self.write_done and self.read_done:
            self.socket.close()
            return True
        else:
            return False

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
                    self.transport.close_tail()
            except error, e:
                print "read error", e
                self.transport.close_tail()
                self.read_done = True
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
        except error, e:
            print "write error", e
            self.transport.close_head()
            self.write_done = True

    def tick(self, now):
        return self.transport.tick(now)

class Acceptor:

    def __init__(self, driver, host, port):
        self.driver = driver
        self.socket = socket()
        self.socket.setblocking(0)
        self.socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        self.socket.bind((host, port))
        self.socket.listen(5)
        self.driver.add(self)

    def closed(self):
        return False

    def fileno(self):
        return self.socket.fileno()

    def reading(self):
        return True

    def writing(self):
        return False

    def readable(self):
        sock, addr = self.socket.accept()
        sock.setblocking(0)
        print "Incoming Connection:", addr
        if sock:
            conn = Connection()
            conn.collect(self.driver.collector)
            transport = Transport(mode=Transport.SERVER)
            transport.bind(conn)
            sasl = transport.sasl()
            sasl.mechanisms("ANONYMOUS")
            sasl.done(SASL.OK)
            sel = Selectable(transport, sock)
            self.driver.add(sel)

    def tick(self, now):
        return None

class Interrupter:

    def __init__(self):
        self.read, self.write = os.pipe()

    def fileno(self):
        return self.read

    def readable(self):
        os.read(self.read, 1024)

    def interrupt(self):
        os.write(self.write, 'x')

class PQueue:

    def __init__(self):
        self.entries = []

    def add(self, task, priority):
        heappush(self.entries, (priority, task))

    def peek(self):
        if self.entries:
            return nsmallest(1, self.entries)[0]
        else:
            return None

    def pop(self):
        if self.entries:
            return heappop(self.entries)
        else:
            return None

    def __nonzero__(self):
        if self.entries:
            return True
        else:
            return False

import proton

TIMER_EVENT = proton.EventType(10000, "on_timer")

class Timer:

    def __init__(self, collector):
        self.collector = collector
        self.tasks = PQueue()

    def schedule(self, task, deadline):
        self.tasks.add(task, deadline)

    def tick(self, now):
        while self.tasks:
            deadline, task = self.tasks.peek()
            if now > deadline:
                self.tasks.pop()
                self.collector.put(task, TIMER_EVENT)
            else:
                return deadline

def _dispatch(ev, handler):
    if ev.clazz == "pn_delivery" and ev.context.released:
        return
    else:
        ev.dispatch(handler)

def _expand(handlers):
    result = []
    for h in handlers:
        if hasattr(h, "handlers"):
            result.extend(h.handlers)
        else:
            result.append(h)
    return result

class Driver(Handler):

    def __init__(self, *handlers):
        self.collector = Collector()
        self.handlers = _expand(handlers)
        self.interrupter = Interrupter()
        self.timer = Timer(self.collector)
        self.selectables = []
        self.now = None
        self.deadline = None
        self._abort = False
        self._exit = False
        self._thread = Thread(target=self.run)
        self._thread.setDaemon(True)

    def schedule(self, task, timeout):
        self.timer.schedule(task, self.now + timeout)

    def abort(self):
        self._abort = True

    def exit(self):
        self._exit = True

    def wakeup(self):
        self.interrupter.interrupt()

    def start(self):
        self._thread.start()

    def join(self):
        self._thread.join()

    def _init_deadline(self):
        self.now = time.time()
        self.deadline = None

    def _update_deadline(self, t):
        if t is None or t < self.now: return
        if self.deadline is None or t < self.deadline:
            self.deadline = t

    @property
    def _timeout(self):
        if self.deadline is None:
            return None
        else:
            return self.deadline - self.now

    def run(self):
        self._init_deadline()
        for h in self.handlers:
            dispatch(h, "on_start", self)

        while True:
            self._init_deadline()

            while True:
                self.process_events()
                if self._abort: return
                self._update_deadline(self.timer.tick(self.now))
                count = self.process_events()
                if self._abort: return
                if not count:
                    break

            reading = [self.interrupter]
            writing = []

            for s in self.selectables[:]:
                if s.reading(): reading.append(s)
                if s.writing(): writing.append(s)
                self._update_deadline(s.tick(self.now))
                if s.closed(): self.selectables.remove(s)

            if self._exit and not self.selectables: return

            try:
                readable, writable, _ = select.select(reading, writing, [], self._timeout)
            except select.error, (err, errtext):
                if err == errno.EINTR:
                    continue
                else:
                    raise

            for s in readable:
                s.readable()
            for s in writable:
                s.writable()

    def process_events(self):
        count = 0

        quiesced = False
        while True:
            ev = self.collector.peek()
            if ev:
                count += 1
                quiesced = False
                _dispatch(ev, self)
                for h in self.get_handlers(ev.context):
                    _dispatch(ev, h)
                self.collector.pop()
            elif quiesced:
                return count
            else:
                for h in self.handlers:
                    dispatch(h, "on_quiesced", self)
                quiesced = True

        return count

    getters = {
        Transport: lambda x: x.connection,
        Delivery: lambda x: x.link,
        Sender: lambda x: x.session,
        Receiver: lambda x: x.session,
        Session: lambda x: x.connection,
    }

    def get_handlers(self, context):
        if hasattr(context, "handlers"):
            return context.handlers
        elif context.__class__ in self.getters:
            parent = self.getters[context.__class__](context)
            return self.get_handlers(parent)
        else:
            return self.handlers

    def on_connection_local_open(self, event):
        conn = event.context
        if conn.state & Endpoint.REMOTE_UNINIT:
            self._connect(conn)

    def _connect(self, conn):
        transport = Transport()
        transport.idle_timeout = 300
        sasl = transport.sasl()
        sasl.mechanisms("ANONYMOUS")
        transport.bind(conn)
        sock = socket()
        sock.setblocking(0)
        hostport = conn.hostname.split(":", 1)
        host = hostport[0]
        if len(hostport) > 1:
            port = int(hostport[1])
        else:
            port = 5672
        sock.connect_ex((host, port))
        selectable = Selectable(transport, sock)
        self.add(selectable)

    def on_timer(self, event):
        event.context()

    def connection(self, *handlers):
        conn = Connection()
        if handlers:
            conn.handlers = _expand(handlers)
        conn.collect(self.collector)
        return conn

    def acceptor(self, host, port):
        return Acceptor(self, host, port)

    def add(self, selectable):
        self.selectables.append(selectable)

class Handshaker(Handler):

    def on_connection_remote_open(self, event):
        conn = event.context
        if conn.state & Endpoint.LOCAL_UNINIT:
            conn.open()

    def on_session_remote_open(self, event):
        ssn = event.context
        if ssn.state & Endpoint.LOCAL_UNINIT:
            ssn.open()

    def on_link_remote_open(self, event):
        link = event.context
        if link.state & Endpoint.LOCAL_UNINIT:
            link.source.copy(link.remote_source)
            link.target.copy(link.remote_target)
            link.open()

    def on_connection_remote_close(self, event):
        conn = event.context
        if not (conn.state & Endpoint.LOCAL_CLOSED):
            conn.close()

    def on_session_remote_close(self, event):
        ssn = event.context
        if not (ssn.state & Endpoint.LOCAL_CLOSED):
            ssn.close()

    def on_link_remote_close(self, event):
        link = event.context
        if not (link.state & Endpoint.LOCAL_CLOSED):
            link.close()

class FlowController(Handler):

    def __init__(self, window):
        self.window = window

    def top_up(self, link):
        delta = self.window - link.credit
        link.flow(delta)

    def on_link_local_open(self, event):
        link = event.context
        if link.is_receiver:
            self.top_up(link)

    def on_link_remote_open(self, event):
        link = event.context
        if link.is_receiver:
            self.top_up(link)

    def on_link_flow(self, event):
        link = event.context
        if link.is_receiver:
            self.top_up(link)

    def on_delivery(self, event):
        delivery = event.context
        if delivery.link.is_receiver:
            self.top_up(delivery.link)

class Row:

    def __init__(self):
        self.links = set()

    def add(self, link):
        self.links.add(link)

    def discard(self, link):
        self.links.discard(link)

    def choose(self):
        if self.links:
            return random.choice(list(self.links))
        else:
            return None

    def __iter__(self):
        return iter(self.links)

    def __nonzero__(self):
        return bool(self.links)


class Router(Handler):

    EMPTY = Row()

    def __init__(self):
        self._outgoing = {}
        self._incoming = {}

    def incoming(self, address):
        return self._incoming.get(address, self.EMPTY)

    def outgoing(self, address):
        return self._outgoing.get(address, self.EMPTY)

    def address(self, link):
        if link.is_sender:
            return link.source.address or link.target.address
        else:
            return link.target.address

    def table(self, link):
        if link.is_sender:
            return self._outgoing
        else:
            return self._incoming

    def add(self, link):
        address = self.address(link)
        table = self.table(link)
        row = table.get(address)
        if row is None:
            row = Row()
            table[address] = row
        row.add(link)

    def remove(self, link):
        address = self.address(link)
        table = self.table(link)
        row = table.get(address)
        if row is not None:
            row.discard(link)
            if not row:
                del table[address]

    def on_link_local_open(self, event):
        self.add(event.context)

    def on_link_local_close(self, event):
        self.remove(event.context)

    def on_link_final(self, event):
        self.remove(event.context)

class Pool(Handler):

    def __init__(self, collector, router=None):
        self.collector = collector
        self._connections = {}
        if router:
            self.outgoing_resolver = lambda address: router.outgoing(address).choose()
            self.incoming_resolver = lambda address: router.incoming(address).choose()
        else:
            self.outgoing_resolver = lambda address: None
            self.incoming_resolver = lambda address: None

    def resolve(self, remote, local, resolver, constructor):
        link = resolver(remote)
        if link is None:
            host = remote[2:].split("/", 1)[0]
            conn = self._connections.get(host)
            if conn is None:
                conn = Connection()
                conn.collect(self.collector)
                conn.hostname = host
                conn.open()
                self._connections[host] = conn

            ssn = conn.session()
            ssn.open()
            link = constructor(ssn, remote, local)
            link.open()
        return link

    def on_transport_closed(self, event):
        transport =  event.context
        conn = transport.connection
        del self._connections[conn.hostname]

    def outgoing(self, target, source=None):
        return self.resolve(target, source, self.outgoing_resolver, self.new_outgoing)

    def incoming(self, source, target=None):
        return self.resolve(source, target, self.incoming_resolver, self.new_incoming)

    def new_outgoing(self, ssn, remote, local):
        snd = ssn.sender("%s-%s" % (local, remote))
        snd.source.address = local
        snd.target.address = remote
        return snd

    def new_incoming(self, ssn, remote, local):
        rcv = ssn.receiver("%s-%s" % (remote, local))
        rcv.source.address = remote
        rcv.target.address = local
        return rcv

class MessageDecoder(Handler):

    def __init__(self, delegate):
        self.__delegate = delegate

    def on_start(self, drv):
        try:
            self.__delegate
        except AttributeError:
            self.__delegate = self
        self.__message = Message()

    def on_delivery(self, event):
        dlv = event.context
        if dlv.link.is_receiver and not dlv.partial:
            encoded = dlv.link.recv(dlv.pending)
            self.__message.decode(encoded)
            try:
                dispatch(self.__delegate, "on_message", dlv.link, self.__message)
                dlv.update(Delivery.ACCEPTED)
            except:
                dlv.update(Delivery.REJECTED)
                traceback.print_exc()
            dlv.settle()

class Address:

    def __init__(self, st):
        self.st = st

    @property
    def host(self):
        return self.st[2:].split("/", 1)[0]

    @property
    def path(self):
        parts = self.st[2:].split("/", 1)
        if len(parts) == 2:
            return parts[1]
        else:
            return ""

    def __repr__(self):
        return "Address(%r)" % self.st

    def __str__(self):
        return self.st

class SendQueue(Handler):

    def __init__(self, address):
        self.address = Address(address)
        self.messages = []
        self.sent = 0

    def on_start(self, drv):
        self.driver = drv
        self.connect()

    def connect(self):
        self.conn = self.driver.connection(self)
        self.conn.hostname = self.address.host
        ssn = self.conn.session()
        snd = ssn.sender(str(self.address))
        snd.target.address = str(self.address)
        ssn.open()
        snd.open()
        self.conn.open()
        self.link = snd

    def put(self, message):
        self.messages.append(message.encode())
        if self.link:
            self.pump(self.link)

    def on_link_flow(self, event):
        link = event.context
        self.pump(link)

    def pump(self, link):
        while self.messages and link.credit > 0:
            dlv = link.delivery(str(self.sent))
            bytes = self.messages.pop(0)
            link.send(bytes)
            dlv.settle()
            self.sent += 1

    def on_transport_closed(self, event):
        conn = event.context.connection
        self.conn = None
        self.link = None
        self.driver.schedule(self.connect, 1)

# XXX: terrible name for this
class RecvQueue(Handler):

    def __init__(self, address, delegate):
        self.address = Address(address)
        self.delegate = delegate
        self.decoder = MessageDecoder(self.delegate)
        self.handlers = [FlowController(1024), self.decoder, self]

    def on_start(self, drv):
        self.driver = drv
        self.decoder.on_start(drv)
        self.connect()

    def connect(self):
        self.conn = self.driver.connection(self)
        self.conn.hostname = self.address.host
        ssn = self.conn.session()
        rcv = ssn.receiver(str(self.address))
        rcv.source.address = str(self.address)
        ssn.open()
        rcv.open()
        self.conn.open()

    def on_transport_closed(self, event):
        conn = event.context.connection
        self.conn = None
        self.driver.schedule(self.connect, 1)
