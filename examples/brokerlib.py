#!/usr/bin/python
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

from xproton import *
from queue import Queue

class Transaction:

  def __init__(self, txn_id):
    self.txn_id = txn_id
    self.work = []

  def add_work(self, doit, undo):
    self.work.append((doit, undo))

  def discharge(self, fail):
    for doit, undo in self.work:
      if fail:
        undo()
      else:
        doit()

class TxnCoordinator:

  def __init__(self):
    self.transactions = {}
    self.next_id = 0

  def declare(self):
    id = str(self.next_id)
    self.next_id += 1
    self.transactions[id] = Transaction(id)
    return id

  def discharge(self, txn_id, fail):
    txn = self.transactions.pop(txn_id)
    txn.discharge(fail)

  def get_transaction(self, state):
    if isinstance(state, TransactionalState):
      txn_id = state.txn_id
      return self.transactions[txn_id]
    else:
      return None

  def target(self):
    return TxnTarget(self)

class TxnTarget:

  def __init__(self, coordinator):
    self.coordinator = coordinator
    self.live = set()
    self.dispatch = {Declare: self.declare, Discharge: self.discharge}

  def configure(self, dfn):
    return dfn

  def capacity(self):
    return True

  def put(self, dtag, xfr, owner=None):
    msg = decode(xfr)
    return self.dispatch[msg.content.__class__](xfr.state, msg)

  def declare(self, state, msg):
    txn = self.coordinator.declare()
    self.live.add(txn)
    return Declared(Binary(txn))

  def discharge(self, state, msg):
    try:
      txn = msg.content.txn_id
      self.coordinator.discharge(txn, msg.content.fail)
      self.live.remove(txn)
      return ACCEPTED
    except KeyError:
      return Rejected(Error("amqp:transaction:unknown-id"))

  def settle(self, dtag, state):
    return state

  def orphaned(self):
    self.close()
    return True

  def close(self):
    while self.live:
      self.coordinator.discharge(self.live.pop(), True)

  def durable(self):
    return False

class Broker:

  def __init__(self, container_id):
    self.container_id = container_id
    self.window = 65536
    self.period = None
    self.frame_size = 4294967295
    self.mechanisms = None
    self.passwords = {}
    self.traces = ()
    self.coordinator = TxnCoordinator()
    self.nodes = {}
    self.sources = {}
    self.targets = {}
    self.dynamic_counter = 0
    self.high = 30
    self.low = 15

  def tick(self, connector):
    self.sasl_tick(connector)

  def sasl_tick(self, connector):
    sasl = pn_connector_sasl(connector)

    while True:
      state = pn_sasl_state(sasl)
      if state == PN_SASL_PASS:
        break
      if state == PN_SASL_CONF:
        pn_sasl_mechanisms(sasl, self.mechanisms)
        pn_sasl_server(sasl)
        continue
      if state in (PN_SASL_IDLE, PN_SASL_FAIL):
        return
      if state == PN_SASL_STEP:
        mech = pn_sasl_remote_mechanisms(sasl)
        if mech == "ANONYMOUS":
          pn_sasl_done(sasl, PN_SASL_OK)
          pn_connector_set_connection(connector, pn_connection())
        if mech == "PLAIN":
          xxx
        continue

    self.amqp_tick(pn_connector_connection(connector))

  def amqp_tick(self, connection):
    state = pn_connection_state(connection)
    if state & PN_LOCAL_UNINIT:
      # XXX channel and frame max
      pn_connection_set_container(connection, self.container_id)
      pn_connection_open(connection)

    ssn = pn_session_head(connection, PN_LOCAL_UNINIT)
    while ssn:
      # XXX session window
      pn_session_open(ssn)
      ssn = pn_session_next(ssn, PN_LOCAL_UNINIT)

    lnk = pn_link_head(connection, PN_LOCAL_UNINIT)
    while lnk:
      if pn_is_sender(lnk):
        self.attach_sender(lnk, connection)
      else:
        self.attach_receiver(lnk, connection)
      lnk = pn_link_next(lnk, PN_LOCAL_UNINIT)

    self.process(connection)

    lnk = pn_link_head(connection, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)
    while lnk:
      if pn_is_sender(lnk):
        self.detach_sender(lnk, connection)
      else:
        self.detach_receiver(lnk, connection)
      lnk = pn_link_next(lnk, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)

    # XXX: need to destroy links

    ssn = pn_session_head(connection, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)
    while ssn:
      pn_session_close(ssn)
      ssn = pn_session_next(ssn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)

    # XXX: need to destroy sessions
    # XXX: need to orphan links

    state = pn_connection_state(connection)
    if (state & PN_LOCAL_ACTIVE) and (state & PN_REMOTE_CLOSED):
      # XXX: should move connection destruction to here
      pn_connection_close(connection)

  def remote_unsettled(self, link):
    result = {}
    d = pn_unsettled_head(link)
    while d:
      rdisp = pn_remote_disp(d)
      result[pn_delivery_tag(d)] = rdisp
      d = pn_unsettled_next(d)
    return result

  def attach_sender(self, link, connection):
    key = (pn_connection_container(connection), pn_link_name(link))
    if key in self.sources:
      source = self.sources[key]

      # XXX: unfortunately this needs to be built first because we
      # don't track remotely vs locally created entries
      remote_unsettled = self.remote_unsettled(link)

      for tag, local in source.resuming():
        # XXX: this whole resume thing is not going to work right now
        # because this will create a new delivery regardless of if the
        # tag exists
        d = pn_delivery(link, tag)
        if local is not None:
          pn_disposition(d, local)

      source.resume(remote_unsettled)
      # XXX: should actually set this to reflect the real source and
      # possibly update the real source
      pn_set_source(link, pn_remote_source(link))
      pn_set_target(link, pn_remote_target(link))
      pn_link_open(link)
    else:
      # XXX: this is just a string
      remote_source = pn_remote_source(link)
      n = self.resolve(remote_source)
      if n is None:
        pn_link_open(link)
        pn_link_close(link)
      else:
        source = n.source()
        local_source = source.configure(remote_source)
        self.sources[key] = source
        pn_set_source(link, local_source)
        pn_set_target(link, pn_remote_target(link))
        pn_link_open(link)

  def attach_receiver(self, link, connection):
    key = (pn_connection_container(connection), pn_link_name(link))
    if key in self.targets:
      target = self.targets[key]

      # XXX: unfortunately this needs to be built first because we
      # don't track remotely vs locally created entries
      remote_unsettled = self.remote_unsettled(link)

      for tag, local in target.resuming():
        # XXX: this is going to create duplicates if a delivery
        # already exists with the same tag
        d = pn_delivery(link, tag)
        pn_disposition(d, local)

      target.resume(remote_unsettled)
      pn_set_source(link, pn_remote_source(link))
      # XXX: should actually set this to reflect the real target and
      # possibly update the real target
      pn_set_target(link, pn_remote_target(link))
      pn_link_open(link)
    else:
      # XXX: this is just a string
      remote_target = pn_remote_target(link)
      n = self.resolve(remote_target)
      if n is None:
        pn_link_open(link)
        pn_link_close(link)
      else:
        target = n.target()
        local_target = target.configure(remote_target)
        self.targets[key] = target
        if target.capacity():
          pn_flow(link, self.high)
        pn_set_source(link, pn_remote_source(link))
        pn_set_target(link, local_target)
        pn_link_open(link)

  def resolve(self, terminus):
    # XXX: need to support full source/target
    #return self.resolvers[terminus.__class__](terminus)
    return self.nodes.get(terminus)

  def resolve_target(self, target):
    return self.resolve_terminus(target)

  def resolve_source(self, source):
    return self.resolve_terminus(source)

  def resolve_terminus(self, terminus):
    t = self.nodes.get(terminus.address)
    if not t and terminus.dynamic:
      t = Queue()
      name = "dynamic-%s" % self.dynamic_counter
      self.dynamic_counter += 1
      self.nodes[name] = t
      # XXX: need to formalize the mutability of the argument
      terminus.address = name
    return t

  def resolve_coordinator(self, target):
    return self.coordinator

  def process(self, connection):
    lnk = pn_link_head(connection, PN_LOCAL_ACTIVE)
    while lnk:
      if pn_is_sender(lnk) and pn_credit(lnk):
        self.process_write(connection, lnk)
      lnk = pn_link_next(lnk, PN_LOCAL_ACTIVE)


    d = pn_work_head(connection)
    while d:
      if pn_readable(d):
        self.process_read(connection, d)
      elif pn_writable(d):
        self.process_write(connection, d)
      if pn_updated(d):
        self.process_disp(connection, d)
      d = pn_work_next(d)

  def process_write(self, connection, link):
    key = (pn_connection_container(connection), pn_link_name(link))
    source = self.sources[key]
    while pn_credit(link) > 0:
      tag, xfr = source.get()
      if xfr is None:
        pn_drained(link)
        break
      else:
        pn_delivery(link, tag)
        cd = pn_send(link, xfr)
        assert cd == len(xfr), cd
        if not pn_advance(link):
          assert False, "this should never happen %s" % pn_credit(link)
        # XXX: snd_settle_mode
        # XXX: message_format?

  def process_read(self, connection, delivery):
    link = pn_link(delivery)
    key = (pn_connection_container(connection), pn_link_name(link))
    target = self.targets[key]

    tag = pn_delivery_tag(delivery)
    cd, xfr = pn_recv(link, pn_pending(delivery))
    assert cd == PN_EOS or cd >= 0, cd
    if cd != PN_EOS:
      cd, tmp = pn_recv(link, 1024)
      assert cd == PN_EOS and tmp == "", (cd, tmp)
    pn_advance(link)

    # XXX: transactions
    txn = None
    # if not isinstance(target, TxnTarget):
    #   if xfr.state:
    #     txn = self.coordinator.get_transaction(xfr.state)
    #   else:
    #     txn = None
    # else:
    #   txn = None
    disp = target.put(tag, xfr, owner=txn)
    # if txn:
    #   xdisp = TransactionalState(txn.txn_id, disp)
    #   def doit(t=xfr.delivery_tag, d=disp, xd=xdisp):
    #     target.settle(t, d)
    #     link.settle(t, xd)
    #   def undo(t=xfr.delivery_tag):
    #     target.settle(t, None)
    #     link.settle(t, None)
    #   txn.add_work(doit, undo)
    #   disp=xdisp
    pn_disposition(delivery, disp)
#    if not txn:
      # XXX: rcv_settle_mode
      # XXX: enums
      # if link.rcv_settle_mode == 0:
      #   pn_settle(delivery)

    credit = pn_credit(link)
    if target.capacity() and credit < self.low:
      pn_flow(link, self.high - credit)

  def process_disp(self, connection, delivery):
    # XXX: sending case
    # for t, l, r in link.get_remote(modified=True):
    #   if l.resumed:
    #     link.settle(t, None)
    #   elif r.settled or r.state is not None:
    #     def doit(t=t, s=r.state):
    #       state = source.settle(t, r.state)
    #       link.settle(t, state)
    #     def undo(t=t):
    #       pass
    #     if r.state:
    #       txn = self.coordinator.get_transaction(r.state)
    #     else:
    #       txn = None
    #     if txn:
    #       txn.add_work(doit, undo)
    #     else:
    #       doit()
    #   r.modified = False

    link = pn_link(delivery)
    key = (pn_connection_container(connection), pn_link_name(link))
    tag = pn_delivery_tag(delivery)
    rdisp = pn_remote_disp(delivery)
    if pn_is_sender(link):
      source = self.sources[key]
      if pn_remote_settled(delivery) or rdisp:
        state = source.settle(tag, rdisp)
        pn_disposition(delivery, state)
        pn_settle(delivery)
      else:
        pn_clear(delivery)
    elif pn_remote_settled(delivery): # XXX: and not transactional
      target = self.targets[key]
      state = target.settle(tag, pn_local_disp(delivery))
      pn_disposition(delivery, state)
      pn_settle(delivery)
    else:
      pn_clear(delivery)

  def orphan_sender(self, link, connection):
    if link.source is None: return
    key = (connection.container_id, link.name)
    source = self.sources[key]
    if source.orphaned():
      del self.sources[key]
      link.source = None
      link.target = None

  def orphan_receiver(self, link, connection):
    if link.target is None: return
    key = (connection.container_id, link.name)
    target = self.targets[key]
    if target.orphaned():
      del self.targets[key]
      link.source = None
      link.target = None

  def detach_sender(self, link, connection):
    if pn_source(link):
      key = (pn_connection_container(connection), pn_link_name(link))
      source = self.sources[key]
      if pn_remote_source(link) is None or not source.durable():
        del self.sources[key]
        source.close()
    pn_link_close(link)

  def detach_receiver(self, link, connection):
    if pn_target(link):
      key = (pn_connection_container(connection), pn_link_name(link))
      target = self.targets[key]
      if pn_remote_target(link) is None or not target.durable():
        del self.targets[key]
        target.close()
    pn_link_close(link)
