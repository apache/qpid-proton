############
API Overview
############

=========================
An overview of the model
=========================

Messages are transferred between connected peers over 'links'. At the
sending peer the link is called a sender. At the receiving peer it is
called a receiver. Messages are sent by senders and received by
receivers. Links may have named 'source' and 'target' addresses (for
example to identify the queue from which message were to be received
or to which they were to be sent).

Links are established over sessions. Sessions are established over
connections. Connections are (generally) established between two
uniquely identified containers. Though a connection can have multiple
sessions, often this is not needed. The container API allows you to
ignore sessions unless you actually require them.

The sending of a message over a link is called a delivery. The message
is the content sent, including all meta-data such as headers and
annotations. The delivery is the protocol exchange associated with the
transfer of that content.

To indicate that a delivery is complete, either the sender or the
receiver 'settles' it. When the other side learns that it has been
settled, they will no longer communicate about that delivery. The
receiver can also indicate whether they accept or reject the
message.

Three different delivery levels or 'guarantees' can be achieved:
at-most-once, at-least-once or exactly-once. See
:ref:`delivery-guarantees` for more detail.

=======================================================
A summary of the most commonly used classes and members
=======================================================

A brief summary of some of the key classes follows.

The :py:class:`~proton.reactor.Container` class is a convenient entry
point into the API, allowing connections and links to be
established. Applications are structured as one or more event
handlers. Handlers can be set at Container, Connection, or Link
scope. Messages are sent by establishing an approprate sender and
invoking its :py:meth:`~proton.Sender.send()` method. This is
typically done when the sender is sendable, a condition indicated by
the :py:meth:`~proton.handlers.MessagingHandler.on_sendable()` event, to
avoid execessive build up of messages. Messages can be received by
establishing an appropriate receiver and handling the
:py:meth:`~proton.handlers.MessagingHandler.on_message()` event.

.. autoclass:: proton.reactor.Container
    :show-inheritance: proton.reactor.Reactor
    :members: connect, create_receiver, create_sender, run, schedule
    :undoc-members:

    .. py:attribute:: container_id

       The identifier used to identify this container in any
       connections it establishes. Container names should be
       unique. By default a UUID will be used.

    The :py:meth:`~proton.reactor.Container.connect()` method returns
    an instance of :py:class:`~proton.Connection`, the
    :py:meth:`~proton.reactor.Container.create_receiver()` method
    returns an instance of :py:class:`~proton.Receiver` and the
    :py:meth:`~proton.reactor.Container.create_sender()` method
    returns an instance of :py:class:`~proton.Sender`.

.. autoclass:: proton.Connection
    :members: open, close, state, session, hostname, container,
              remote_container, remote_desired_capabilities, remote_hostname, remote_offered_capabilities , remote_properties
    :undoc-members:

.. autoclass:: proton.Receiver
    :show-inheritance: proton.Link
    :members: flow, recv, drain, draining
    :undoc-members:

.. autoclass:: proton.Sender
    :show-inheritance: proton.Link
    :members: offered, send
    :undoc-members:

.. autoclass:: proton.Link
    :members: name, state, is_sender, is_receiver,
              credit, queued, session, connection,
              source, target, remote_source, remote_target
    :undoc-members:

    The :py:meth:`~proton.Link.source()`,
    :py:meth:`~proton.Link.target()`,
    :py:meth:`~proton.Link.remote_source()` and
    :py:meth:`~proton.Link.remote_target()` methods all return an
    instance of :py:class:`~proton.Terminus`.


.. autoclass:: proton.Delivery
    :members: update, settle, settled, remote_state, local_state, partial, readable, writable,
              link, session, connection
    :undoc-members:

.. autoclass:: proton.handlers.MessagingHandler
    :members: on_start, on_reactor_init,
              on_message,
              on_accepted,
              on_rejected,
              on_settled,
              on_sendable,
              on_connection_error,
              on_link_error,
              on_session_error,
              on_disconnected,
              accept, reject, release, settle
    :undoc-members:

.. autoclass:: proton.Event
    :members: delivery, link, receiver, sender, session, connection, reactor, context
    :undoc-members:

.. autoclass:: proton.Message
    :members: address, id, priority, subject, ttl, reply_to, correlation_id, durable, user_id,
              content_type, content_encoding, creation_time, expiry_time, delivery_count, first_acquirer,
              group_id, group_sequence, reply_to_group_id,
              send, recv, encode, decode
    :undoc-members:

.. autoclass:: proton.Terminus
    :members: address, dynamic, properties, capabilities, filter
    :undoc-members:

.. _delivery-guarantees:

===================
Delivery guarantees
===================

For at-most-once, the sender settles the message as soon as it sends
it. If the connection is lost before the message is received by the
receiver, the message will not be delivered.

For at-least-once, the receiver accepts and settles the message on
receipt. If the connection is lost before the sender is informed of
the settlement, then the delivery is considered in-doubt and should be
retried. This will ensure it eventually gets delivered (provided of
course the connection and link can be reestablished). It may mean that
it is delivered multiple times though.

Finally, for exactly-once, the receiver accepts the message but
doesn't settle it. The sender settles once it is aware that the
receiver accepted it. In this way the receiver retains knowledge of an
accepted message until it is sure the sender knows it has been
accepted. If the connection is lost before settlement, the receiver
informs the sender of all the unsettled deliveries it knows about, and
from this the sender can deduce which need to be redelivered. The
sender likewise informs the receiver which deliveries it knows about,
from which the receiver can deduce which have already been settled.
