Introduction     {#mainpage}
============

This is the C++ API for the proton AMQP protocol engine. It allows you to write
client and server applications that send and receive AMQP messages.

The best way to start is with the \ref tutorial "tutorial".

An overview of the model
------------------------

Messages are transferred between connected peers over 'links'. At the sending
peer the link is called a sender. At the receiving peer it is called a
receiver. Messages are sent by senders and received by receivers. Links may have
named 'source' and 'target' addresses (see "sources and targets" below.)

Links are established over sessions. Sessions are established over
connections. Connections are (generally) established between two uniquely
identified containers. Though a connection can have multiple sessions, often
this is not needed. The container API allows you to ignore sessions unless you
actually require them.

The sending of a message over a link is called a delivery. The message
is the content sent, including all meta-data such as headers and
annotations. The delivery is the protocol exchange associated with the
transfer of that content.

To indicate that a delivery is complete, either the sender or the
receiver 'settles' it. When the other side learns that it has been
settled, they will no longer communicate about that delivery. The
receiver can also indicate whether they accept or reject the
message.

Three different delivery levels or 'guarantees' can be achieved: at-most-once,
at-least-once or exactly-once. See below for details.

### Sources and targets ###

Every link has two addresses, *source* and *target*. The most common pattern for
using these addresses is as follows:

When a client creates a *receiver* link, it sets the *source* address. This
means "I want to receive messages from this source." This is often referred to
as "subscribing" to the source. When a client creates a *sender* link, it sets
the *target* address. This means "I want to send to this target."

In the case of a broker, the source or target usually refers to a queue or
topic. In general they can refer to any AMQP-capable node.

In the *request-response* pattern, a request message carries a *reply-to*
address for the response message. This can be any AMQP address, but it is often
useful to create a temporary address for just the response message.

The most common approach is for the client to create a *receiver* for the
response with the *dynamic* flag set. This asks the server to generate a unique
*source* address automatically and discard it when the link closes. The client
uses this "dynamic" source address as the reply-to when it sends the request,
and the response is delivered to the client's dynamic receiver.

In the case of a broker a dynamic address usually corresponds to a temporary
queue but any AMQP request-response server can use this technique. The \ref
server_direct.cpp example illustrates how to implement a queueless
request-response server.

Commonly used classes
---------------------

A brief summary of some of the key classes follows.

The `proton::container` class is a convenient entry point into the API, allowing
connections and links to be established. Applications are structured as one or
more event handlers.

The easiest way to implement a handler is to define a subclass of
`proton::messaging_handler` and over-ride the event handling member functions
that interest you.

To send messages, call `proton::container::create_sender()` to create a
`proton::sender` and then call `proton::sender::send()`. This is typically done
when the sender is *sendable*, a condition indicated by the
`proton::messaging_handler::on_sendable()` event, to avoid execessive build up
of messages.

To receive messages, call `proton::container::create_receiver()` to create a
`proton::receiver`.  When messages are recieved the
`proton::messaging_handler::on_message()` event handler function will be called.

Other key classes:

- proton::message represents an AMQP message, which has a body, headers and other properties.
- proton::connection represents a connection to a remote AMQP endpoint.
- proton::delivery holds the delivery status of a message.
- proton::event carries details of an event.

The library provides intuitive default conversions between AMQP and C++ types
for message data q, but also allows detailed examination and construction of
complex AMQP types. For details on converting between AMQP and C++ data types
see the \ref encode_decode.cpp example and classes `proton::encoder`,
`proton::decoder` and `proton::value`.

Delivery Guarantees
-------------------

For *at-most-once*, the sender settles the message as soon as it sends
it. If the connection is lost before the message is received by the
receiver, the message will not be delivered.

For *at-least-once*, the receiver accepts and settles the message on
receipt. If the connection is lost before the sender is informed of
the settlement, then the delivery is considered in-doubt and should be
retried. This will ensure it eventually gets delivered (provided of
course the connection and link can be reestablished). It may mean that
it is delivered multiple times though.

Finally, for *exactly-once*, the receiver accepts the message but
doesn't settle it. The sender settles once it is aware that the
receiver accepted it. In this way the receiver retains knowledge of an
accepted message until it is sure the sender knows it has been
accepted. If the connection is lost before settlement, the receiver
informs the sender of all the unsettled deliveries it knows about, and
from this the sender can deduce which need to be redelivered. The
sender likewise informs the receiver which deliveries it knows about,
from which the receiver can deduce which have already been settled.
