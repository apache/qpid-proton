# Introduction {#mainpage}

This is the C++ API for the Proton AMQP protocol engine. It allows you
to write client and server applications that send and receive AMQP
messages.

The best way to start is with the @ref tutorial

## An overview of the AMQP model

Messages are transferred between connected peers over *links*. The
sending end of a link is a `proton::sender`, and the receiving end is
a `proton::receiver`.  Links have named 'source' and 'target'
addresses.  See "Sources and Targets" below for more information.

Links are grouped in a `proton::session`. Messages for links in the
same session are sent sequentially.  Messages on different sessions
can be interleaved, so a large message being sent on one session does
not block messages being sent on other sessions.

Sessions are created over a `proton::connection`, which represents the
network connection. You can create links directly on a connection
using its default session if you don't need multiple sessions.

`proton::message` represents the message: the body (content), properties,
headers and annotations. A `proton::delivery` represents the act of transferring
a message over a link. The receiver acknowledges the delivery by accepting or
rejecting it. The delivery is *settled* when both ends are done with it.
Different settlement methods give different levels of reliability:
*at-most-once*, *at-least-once*, and *exactly-once*. See "Delivery Guarantees"
below for details.

## Sources and targets

Every link has two addresses, *source* and *target*. The most common
pattern for using these addresses is as follows:

When a client creates a *receiver* link, it sets the *source*
address. This means "I want to receive messages from this source."
This is often referred to as "subscribing" to the source. When a
client creates a *sender* link, it sets the *target* address. This
means "I want to send to this target."

In the case of a broker, the source or target usually refers to a
queue or topic. In general they can refer to any AMQP-capable node.

In the *request-response* pattern, a request message carries a
*reply-to* address for the response message. This can be any AMQP
address, but it is often useful to create a temporary address for just
the response message.

The most common approach is for the client to create a *receiver* for
the response with the *dynamic* flag set. This asks the server to
generate a unique *source* address automatically and discard it when
the link closes. The client uses this "dynamic" source address as the
reply-to when it sends the request, and the response is delivered to
the client's dynamic receiver.

In the case of a broker, a dynamic address usually corresponds to a
temporary queue, but any AMQP request-response server can use this
technique. The @ref server_direct.cpp example illustrates how to
implement a queueless request-response server.

## Delivery guarantees

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

## Anatomy of a Proton application

To send AMQP commands, call methods on classes like `proton::connection`,
`proton::sender`, `proton::receiver`, or `proton::delivery`.

To handle incoming commands, subclass the `proton::handler` interface. The
handler member functions are called when AMQP protocol events occur on a
connection. For example `proton::handler::on_message` is called when a message
is received.

Messages are represented by `proton::message`. AMQP defines a type
encoding that you can use for interoperability, but you can also use
any encoding you wish and pass binary data as the
`proton::message::body`. `proton::value` and `proton::scalar` provide
conversion between AMQP and C++ data types.

There are several ways to manage handlers and AMQP objects, for different types
of application. All of them use the same `proton::handler` sub-classes so code
can be re-used if you change your approach.

### %proton::container - easy single-threaded applications

`proton::container` is an easy way to get started with single threaded
applications. It manages the low-level socket connection and polling for you so
you can write portable applications using just the @ref proton API. You an use
proton::connection::connect() and proton::container::listen() to create
connections. The container polls multiple connections and calls protocol eventsa
on your `proton::handler` sub-classes.

The container has two limitations:
- it is not thread-safe, all container work must happen in a single thread.
- it provides portable IO handling for you but cannot be used for different types of IO.

### %proton::mt::controller - multi-threaded applications

The proton::controller is similar to the proton::container but it can process
connections in multiple threads concurrently. It uses the `proton::handler` like
the container. If you follow the recommended model you can re-use statefull,
per-connection, handlers with the controller. See @ref mt_page and the examples
@ref mt/broker.cpp and @ref mt/epoll\_controller.cpp

Default controller IO implementations are provided but you can also implement
your own controller using the proton::io::connection_engine, read on...

### %proton::io::connection_engine - integrating with foreign IO

The `proton::io::connection_engine` is different from the other proton APIs. You
might think of it as more like an SPI (Service Provided Interface).

The engine provides a very low-level way of driving a proton::handler: You feed
raw byte-sequence fragments of an AMQP-encoded stream to the engine and it
converts that into calls on a proton::handler. The engine provides you with
outgoing protocol stream bytes in return.

The engine is deliberately very simple and low level. It does no IO, no
thread-related locking, and is written in simple C++98 compatible code.

You can use the engine directly to connect your application to any kind of IO
framework or library, memory-based streams or any other source/sink for byte
stream data.

You can also use the engine to build a custom implementation of
proton::mt::controller and proton::mt::work\_queue so portable proton
applications using the controller can run without modification on your platform.
