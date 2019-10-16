####################################
Qpid Proton Python API Documentation
####################################

The Proton module provides a Python 2.7 and 3.x API for Qpid Proton. It enables a developer to write Python applications
that send and receive AMQP messages.

*******
Modules
*******
.. toctree::
   :maxdepth: 2

   proton
   proton.handlers
   proton.reactor
   proton.utils

*****************************************
About AMQP and the Qpid Proton Python API
*****************************************

.. toctree::
   :maxdepth: 1

   overview
   types
   tutorial

Key API Features
================

 * Event-driven API
 * SSL/TLS secured communication
 * SASL authentication
 * Automatic reconnect and failover
 * Seamless conversion between AMQP and Python data types
 * AMQP 1.0

Basic API Concepts
==================

The Qpid Python client API and library allows applications to send and receive AMQP messages. See :ref:`overview`
for a more detailed explanation.

Containers
----------

Messages are transferred between connected peers (or nodes) using **senders** and **receivers**. Each sender
or receiver is established over a **connection**. Each connection is established between two unique **containers**,
the entry point for the API. The container class :class:`proton.reactor.Container` is found in the ``proton.reactor``
module.

Connections
-----------

A **connection** object tracks the status of an AMQP connection. For applications which *don't* require either
authorization or encryption, these may be automatically created by convenience methods
:meth:`proton.reactor.Container.create_sender` and/or :meth:`proton.reactor.Container.create_receiver`.
However, for applications which *do* require either of these services, a connection object should be created
using the convenience method :meth:`proton.reactor.Container.connect`, providing the required parameters.
This object should then be passed to the convenience methods :meth:`proton.reactor.Container.create_sender`
and/or :meth:`proton.reactor.Container.create_receiver` as needed. The connection class may be found at
:class:`proton.Connection`.

Senders
-------

The peer that sends messages uses a **sender** to send messages, which includes the target queue or topic which is
to receive the messages. The sender may be found at :class:`proton.Sender`. Note that senders are most commonly
obtained by using the convenience method :meth:`proton.reactor.Container.create_sender`.

Receivers
---------

The peer that receives messages uses a **receiver** to receive messages, and includes a source queue or topic from
which to receive messages. The receiver may be found at :class:`proton.Receiver`. Note that senders are most commonly
obtained by using the convenience method :meth:`proton.reactor.Container.create_receiver`.

Message Delivery
----------------

The process of sending a message is known as **delivery**. Each sent message has a delivery object which tracks the
status of the message as it is sent from the sender to the receiver. This also includes actions such as settling the
delivery (ie causing the delivery status to be forgotten when it is no longer needed). The delivery class may be found
at :class:`proton.Delivery`. The delivery object is most commonly obtained
when a message-related event occurs through the event object. See `Event Handlers`_ below.

Event Handlers
--------------

A **handler** is a class that handles events associated with the sending and receiving of messages. This includes
callbacks for events such as the arrival of a new message, error conditions that might arise, and the closing
of the connection over which messages are sent. An application developer must handle some key events to
successfully send and receive messages. When an event handler callback is called by the library, an Event object is
passed to it which contains an object associated with the event. For example,
when a message is received, the event object will have the property ``event.message`` by which the message itself may
be obtained. See :class:`proton.Event` for more details.

The following are some of the important event callbacks that may be implemented by a developer:

* **on_start()**: This indicates that the event loop in the container has started, and that a new sender and/or
    receiver may now be created.

To send a message, the following events may need to be handled:

* **on_sendable()**: This callback indicates that send credit has now been set by the receiver, and that a message may
    now be sent.
* **on_accepted()**: This callback indicates that a message has been received and accepted by the receiving peer.

To receive a message, the following event may need to be handled:

* **on_message()**: This callback indicates that a message has been received. The message and its delivery object may
    be retreived, and if needed, the message can be either accepted or rejected.

Many other events exist for the handling of transactions and other message events and errors, and if present in
your handler will be called as the corresponding events arise. See the :ref:`tutorial` for examples of handling
some other events.

Several event handlers are provided which provide default behavior for most events. These may be found in the
``proton.handlers`` module. The :class:`proton.handlers.MessagingHandler` is the most commonly used handler for
non-transactional applications. Developers would typically directly inherit from this handler to provide the
application's event behavior, and override callbacks as needed to provide additional behavior they may require.

AMQP types
----------
The types defined by the AMQP specification are mapped to either native Python types or to special proton classes
which represent the AMQP type. See :ref:`types` for a summary.

Examples
--------

Several examples may be found in the
`Apache Qpid Proton Examples <https://qpid.apache.org/releases/qpid-proton-0.28.0/proton/python/examples/index.html>`_
whcih illustrate the techniques and concepts of sending messages. They are also present in the source. These make
an excellent starting point for developers new to this API. Make sure to read the README file, which gives
instructions on how to run them and a brief explanation of each example.

Tutorial
--------

See this :ref:`tutorial` on using the API.

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

.. * :ref:`modindex`
    Can't get this to generate, so commenting out. Also, not that useful in this case, as modules are listed
    in the contents above in full.
