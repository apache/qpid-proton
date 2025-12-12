#!/usr/bin/env python3
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

from abc import ABC, abstractmethod
from collections import deque, namedtuple
from dataclasses import dataclass
import optparse
import uuid

from typing import Optional, Union

from proton import (Condition, Delivery, Described, Disposition, DispositionType,
                    Endpoint, Link, Sender, Message, Terminus, DeclaredDisposition,
                    RejectedDisposition, TransactionalDisposition)
from proton.handlers import MessagingHandler
from proton.reactor import Container


class Queue:
    def __init__(self, broker: 'Broker', name: str, dynamic: bool = False):
        self.broker = broker
        self.name: str = name
        self.dynamic: bool = dynamic
        self.queue: deque[Message] = deque()
        self.consumers: list[Sender] = []

    def subscribe(self, consumer: Sender):
        self.consumers.append(consumer)

    def unsubscribe(self, consumer: Sender):
        if consumer in self.consumers:
            self.consumers.remove(consumer)

    def removable(self):
        return len(self.consumers) == 0 and (self.dynamic or len(self.queue) == 0)

    def publish(self, message: Message):
        self.queue.append(message)
        self.dispatch()

    def dispatch(self, consumer: Optional[Sender] = None):
        if consumer:
            c = [consumer]
        else:
            c = self.consumers
        while self._deliver_to(c):
            pass

    def _deliver_to(self, consumers: list[Sender]):
        try:
            result = False
            for c in consumers:
                if c.credit:
                    message = self.queue.popleft()
                    self.broker.deliver(c, message)
                    result = True
            return result
        except IndexError:  # no more messages
            return False


UnsettledDelivery = namedtuple('UnsettledDelivery', ['address', 'message'])


class TransactionAction(ABC):
    @abstractmethod
    def commit(self, broker): ...

    @abstractmethod
    def rollback(self, broker): ...


@dataclass
class QueueMessage(TransactionAction):
    delivery: Delivery
    message: Message
    address: str

    def commit(self, broker):
        broker.publish(self.message, self.address)
        self.delivery.settle()

    def rollback(self, broker):
        pass


@dataclass
class RejectDelivery(TransactionAction):
    delivery: Delivery

    def commit(self, broker):
        self.delivery.settle()

    def rollback(self, broker):
        pass


@dataclass
class DeliveryUpdate(TransactionAction):
    delivery: Delivery
    outcome: DispositionType

    def commit(self, broker):
        broker.delivery_update(self.delivery, self.outcome)

    def rollback(self, broker):
        broker.delivery_update(self.delivery, Disposition.RELEASED)


class Broker(MessagingHandler):
    def __init__(self, url, verbose, redelivery_limit):
        super().__init__(auto_accept=False)
        self.verbose = verbose
        self.url = url
        self.redelivery_limit = redelivery_limit
        self.queues: dict[str, Queue] = {}
        self.unsettled_deliveries: dict[Delivery, UnsettledDelivery] = {}
        self.txns: dict[bytes, list[TransactionAction]] = {}
        self.acceptor = None

    def _verbose_print(self, message):
        if self.verbose:
            print(message)

    def _queue(self, address, dynamic=False):
        if address not in self.queues:
            self.queues[address] = Queue(self, address, dynamic)
            self._verbose_print(f"{address=}: Created")
        return self.queues[address]

    def on_start(self, event):
        self.acceptor = event.container.listen(self.url)

    def on_connection_opening(self, event):
        event.connection.offered_capabilities = 'ANONYMOUS-RELAY'

    def on_link_opening(self, event):
        link = event.link
        if link.is_sender:
            dynamic = link.remote_source.dynamic
            if dynamic or link.remote_source.address:
                address = str(uuid.uuid4()) if dynamic else link.remote_source.address
                link.source.address = address
                self._queue(address, dynamic).subscribe(link)
                self._verbose_print(f"{link=}: Subscribed: {address=}")
        elif link.remote_target.type == Terminus.COORDINATOR:
            # Set up transaction coordinator
            # Should check for compatible capabilities
            # requested = link.remote_target.capabilities.get_object()
            link.target.type = Terminus.COORDINATOR
            link.target.copy(link.remote_target)
            link._txns = set()
        elif link.remote_target.address:
            link.target.address = link.remote_target.address

    def _unsubscribe(self, link):
        address = link.source.address
        if address in self.queues:
            q = self.queues[address]
            q.unsubscribe(link)
            self._verbose_print(f"{link=}: Unsubscribed: {address=}")
            if q.removable():
                del q
                self._verbose_print(f"{address=}: Removed")

    def _modify_delivery(self, delivery: Delivery, message: Message, address: str):
        disposition = delivery.remote
        # If not deliverable don't requeue
        if disposition.undeliverable:
            self._verbose_print(f"{delivery.tag=}: Modified: Undeliverable: {message.id=}")
            # Don't requeue the message
            return
        # Check if we need to update the delivery count
        if disposition.failed:
            if message.delivery_count >= self.redelivery_limit:
                self._verbose_print(f"{delivery.tag=}: Modified: Redelivery limit exceeded: {message.id=}")
                # Don't requeue the message
                return
            # Update the delivery count
            message.delivery_count += 1
        # Requeue the message from the delivery
        self._verbose_print(f"{delivery.tag=}: Modified: {message.id=} Requeued: {address=}")
        self._queue(address).publish(message)

    def delivery_update(self, delivery: Delivery, outcome: Union[int, DispositionType]):
        unsettled_delivery = self.unsettled_deliveries[delivery]
        message = unsettled_delivery.message
        address = unsettled_delivery.address
        if outcome == Disposition.ACCEPTED:
            self._verbose_print(f"{delivery.tag=}: Accepted: {message.id=}")
            # Delivery was accepted - nothing further to do
        elif outcome == Disposition.REJECTED:
            self._verbose_print(f"{delivery.tag=}: Rejected: {message.id=}")
            # Delivery was rejected - nothing further to do
        elif outcome == Disposition.RELEASED:
            self._verbose_print(f"{delivery.tag=}: Released: {message.id=} Requeued: {address=}")
            # Requeue the message from the delivery
            self._queue(address).publish(message)
        elif outcome == Disposition.MODIFIED:
            self._modify_delivery(delivery, message, address)
        delivery.settle()
        del unsettled_delivery

    def _declare_txn(self):
        tid = bytes(uuid.uuid4().bytes)
        self.txns[tid] = []
        return tid

    def _discharge_txn(self, tid, failed):
        if not failed:
            # Commit
            self._verbose_print(f"{tid=}: Commit")
            for action in self.txns[tid]:
                action.commit(self)
        else:
            # Rollback
            self._verbose_print(f"{tid=}: Rollback")
            for action in self.txns[tid]:
                action.rollback(self)
        del self.txns[tid]

    def _coordinator_message(self, msg, delivery):
        body = msg.body
        if isinstance(body, Described):
            link = delivery.link
            d = body.descriptor
            if d == "amqp:declare:list":
                # Allocate transaction id
                tid = self._declare_txn()
                self._verbose_print(f"{tid=}: Declare")
                delivery.local = DeclaredDisposition(tid)
                link._txns.add(tid)
            elif d == "amqp:discharge:list":
                # Always accept commit/abort!
                value = body.value
                tid = bytes(value[0])
                failed = bool(value[1])
                if tid in link._txns:
                    self._discharge_txn(tid, failed)
                    delivery.update(Disposition.ACCEPTED)
                    link._txns.remove(tid)
                else:
                    self._verbose_print(f"{tid=}: Discharge unknown txn-id: {failed=}")
                    delivery.local.condition = Condition('amqp:transaction:unknown-id')
                    delivery.update(Disposition.REJECTED)
        delivery.settle()

    def on_link_closing(self, event):
        link = event.link
        if link.is_sender:
            self._unsubscribe(link)
        elif link.target.type == Terminus.COORDINATOR:
            # Abort any remaining active transactions
            for tid in link._txns:
                self._discharge_txn(tid, failed=True)
            link._txns.clear()

    def _remove_stale_consumers(self, connection):
        for link in connection.links(Endpoint.REMOTE_ACTIVE):
            if link.is_sender:
                self._unsubscribe(link)

    def _abort_active_transactions(self, connection):
        for link in connection.links(Endpoint.LOCAL_ACTIVE):
            if link.target.type == Terminus.COORDINATOR:
                # Abort any remaining active transactions
                for tid in link._txns:
                    self._discharge_txn(tid, failed=True)
                link._txns.clear()

    def on_connection_closing(self, event):
        connection = event.connection
        self._remove_stale_consumers(connection)
        self._abort_active_transactions(connection)

    def on_disconnected(self, event):
        connection = event.connection
        self._remove_stale_consumers(connection)
        self._abort_active_transactions(connection)

    def on_sendable(self, event):
        link: Link = event.link
        self._queue(link.source.address).dispatch(link)

    def on_message(self, event):
        link: Link = event.link
        delivery: Delivery = event.delivery

        message = event.message
        if link.target.type == Terminus.COORDINATOR:
            # Deal with special transaction messages
            self._coordinator_message(message, delivery)
            return

        address = link.target.address
        if address is None:
            address = message.address

        ldisposition = None
        if address is None:
            self._verbose_print(f"{message.id=}: Message without address")
            ldisposition = RejectedDisposition(Condition('amqp:link:invalid-address'))
        else:
            ldisposition = Disposition.ACCEPTED

        # Is this a transactioned message?
        rdisposition = delivery.remote
        if rdisposition and rdisposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = rdisposition.id
            if tid in self.txns:
                if address:
                    self._verbose_print(f"{tid=}: Message: {message.id=}")
                    self.txns[tid].append(QueueMessage(delivery, message, address))
                else:
                    self.txns[tid].append(RejectDelivery(delivery))
                delivery.local = TransactionalDisposition(tid, ldisposition)
                delivery.update()
                return
            else:
                self._verbose_print(f"{tid=}: Message: unknown txn-id")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
                delivery.settle()
                return

        if address:
            self.publish(message, address)
        delivery.update(ldisposition)
        delivery.settle()

    def publish(self, message, address):
        queue = self._queue(address)
        queue.publish(message)
        self._verbose_print(f"{message.id=} Queued: {queue.name=}")

    def deliver(self, consumer, message):
        delivery = consumer.send(message)
        address = consumer.source.address
        self.unsettled_deliveries[delivery] = UnsettledDelivery(address, message)
        self._verbose_print(f"{delivery.tag=}: Sent: {message.id=} to {address=}")

    def on_delivery_updated(self, event):
        """Handle all delivery updates for the link."""
        delivery = event.delivery
        disposition = delivery.remote
        # Is this a transactioned delivery update?
        if disposition.type == Disposition.TRANSACTIONAL_STATE:
            tid = disposition.id
            outcome = disposition.outcome_type
            if tid in self.txns:
                self._verbose_print(f"{tid=}: Delivery update: outcome={outcome}")
                self.txns[tid].append(DeliveryUpdate(delivery, outcome))
                return
            else:
                self._verbose_print(f"{tid=}: Delivery update: unknown txn-id")
                delivery.local.condition = Condition('amqp:transaction:unknown-id')
                delivery.update(Disposition.REJECTED)
        else:
            self.delivery_update(delivery, disposition.type)
        # The delivery is settled in every case except a valid transaction
        # where the outcome is not yet known until the transaction is discharged.
        delivery.settle()


def main():
    parser = optparse.OptionParser(usage="usage: %prog [options]")
    parser.add_option("-v", "--verbose", action="store_true", default=False,
                      help="enable verbose output (default %default)")
    parser.add_option("-a", "--address", default="localhost:5672",
                      help="address router listens on (default %default)")
    parser.add_option("-l", "--redelivery-limit", default=5,
                      help="maximum redelivery attempts (default %default)")
    opts, args = parser.parse_args()

    try:
        Container(Broker(opts.address, opts.verbose, opts.redelivery_limit)).run()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
