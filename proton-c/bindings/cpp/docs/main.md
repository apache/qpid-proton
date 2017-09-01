
# Introduction {#mainpage}

The [Qpid Proton](http://qpid.apache.org/proton) C++ library lets you write
clients and servers that send and receive messages using the AMQP protocol, in
C++. It can be used with single-threaded and multi-threaded code.

The @ref tutorial will guide you through some basic example, there are
further [examples](examples.html) on more advanced topics.

## An overview of AMQP

A proton::message has a *body* (the main content), application properties where
you can store additional data, and specific properties defined by the AMQP
protocol.

Messages are transferred over *links*. The sending end of a link is a
proton::sender, the receiving end is a proton::receiver.  Links have a
*source* and *target* address, as explained [below](#sources-and-targets)

Links are grouped in a `proton::session`. Messages in the same session are sent
sequentially, those on different sessions can be interleaved. A large message
being sent on one session does not block messages being sent on other sessions.

Sessions are on a `proton::connection`. If you don't need multiple sessions, a
proton::connection will create links directly using a default session.

A `proton::delivery` represents the delivery of a message, and allows the
receiver to accept or reject it. The sender can use a `proton::tracker` to track
the status of a sent message and find out if it was accepted.

A delivery is *settled* when both ends are done with it.  Different settlement
methods give different levels of reliability: *at-most-once*, *at-least-once*,
and *exactly-once*. See [below](#delivery-guarantees)

## Anatomy of a Proton application

proton::container is the top-level object in a proton application.
A client uses proton::container::connect() to establish connections, a server
uses proton::container::listen() to accept connections.de

Proton is an event-driven API. You implement a subclass of
`proton::messaging_handler` and over-ride functions to handle AMQP events, such
as `on_container_open()` or `on_message()`. Each proton::connection is
associated with a handler for its events.  proton::container::run() polls all
connections and listeners and dispatches events to your handlers.

A message body can be a string or byte-sequence encoded in any way you
like. However AMQP also provides standard, inter-operable encodings for basic
data types and structures like maps and lists. You can use this encoding for
your message bodies via `proton::value` and `proton::scalar`, which convert C++
types to their AMQP equivalent.

The @ref tutorial will guide you through some basic example, there are
further [examples](examples.html) on more advanced topics.

## Sources and targets

Every link has two addresses, *source* and *target*. The most common pattern for
using these addresses is as follows:

When a client creates a *receiver* link, it sets the *source* address. This
means "I want to receive messages from this source."  This is often referred to
as "subscribing" to the source. When a client creates a *sender* link, it sets
the *target* address. This means "I want to send to this target."

In the case of a broker, the source or target usually refers to a queue or
topic. In general they can refer to any AMQP-capable node.

In the *request-response* pattern, a request message carries a *reply-to*
address for the response message. This can be any AMQP address, but it is often
useful to create a temporary address for the response message. The client
creates a *receiver* with no source address and the *dynamic* flag set. The
server generates a unique *source* address for the receiver, which is discarded
it when the link closes. The client uses this source address as the reply-to
when it sends the request, so the response is delivered to the client's
receiver.

The @ref server_direct.cpp example shows how to implement a request-response
server.

## Delivery guarantees

Proton offers three levels of message delivery guarantee:
*at-most-once*, *at-least-once*, and *exactly-once*.

For *at-most-once*, the sender settles the message as soon as it sends
it. If the connection is lost before the message is received by the
receiver, the message will not be delivered.

For *at-least-once*, the receiver accepts and settles the message on
receipt. If the connection is lost before the sender is informed of
the settlement, then the delivery is considered in-doubt and should be
retried. This will ensure it eventually gets delivered (provided of
course the connection and link can be reestablished). It may mean that
it is delivered multiple times, however.

Finally, for *exactly-once*, the receiver accepts the message but
doesn't settle it. The sender settles once it is aware that the
receiver accepted it. In this way the receiver retains knowledge of an
accepted message until it is sure the sender knows it has been
accepted. If the connection is lost before settlement, the receiver
informs the sender of all the unsettled deliveries it knows about, and
from this the sender can deduce which need to be redelivered. The
sender likewise informs the receiver which deliveries it knows about,
from which the receiver can deduce which have already been settled.

