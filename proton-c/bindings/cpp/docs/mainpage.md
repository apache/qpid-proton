# Introduction {#mainpage}

This is the C++ API for the Proton AMQP protocol engine. It allows you
to write client and server applications that send and receive AMQP
messages.

The best way to start is with the \ref tutorial "tutorial".

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

`proton::message` represents the message: the body (content), headers,
annotations, and so on. A `proton::delivery` represents the act of
transferring a message over a link. The receiver acknowledges the
delivery by accepting or rejecting it. The delivery is *settled* when
both ends are done with it.  Different settlement methods give
different levels of reliability: *at-most-once*, *at-least-once*, and
*exactly-once*. See "Delivery Guarantees" below for details.

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
technique. The \ref server_direct.cpp example illustrates how to
implement a queueless request-response server.

## Anatomy of a Proton application

To send AMQP commands, call methods on classes like
`proton::connection`, `proton::sender`, `proton::receiver`, or
`proton::delivery`. To handle incoming commands, implement the
`proton::handler` interface. The handler receives calls like
`on_message` (a message is received), `on_link_open` (a link is
opened), and `on_sendable` (a link is ready to send messages).

Messages are represented by `proton::message`. AMQP defines a type
encoding that you can use for interoperability, but you can also use
any encoding you wish and pass binary data as the
`proton::message::body`. `proton::value` and `proton::scalar` provide
conversion between AMQP and C++ data types.

<!-- See the example \ref encode_decode.cpp. -->

There are two alternative ways to manage handlers and AMQP objects,
the `proton::container` and the `proton::connection_engine`. You can
code your application so that it can be run with either. Say you find
the `proton::container` easier to get started but later need more
flexibility.  You can switch to the `proton::connection_engine` with
little or no change to your handlers.

### %proton::container

`proton::container` is a *reactor* framework that manages multiple
connections and dispatches events to handlers. You implement
`proton::handler` with your logic to react to events, and register it
with the container. The container lets you open multiple connections
and links, receive incoming connections and links, and send, receive,
and acknowledge messages.

The reactor handles IO for multiple connections using sockets and
`poll`. It dispatches events to your handlers in a single thread,
where you call `proton::container::run`. The container is not
thread-safe: once it is running you can only operate on Proton objects
from your handler methods.

### %proton::connection_engine

`proton::connection_engine` dispatches events for a *single
connection*. The subclass `proton::io::socket::engine` does
socket-based IO. An application with a single connection is just like
using `proton::container` except you attach your handler to a
`proton::io::socket::engine` instead. You can compare examples, such as
\ref helloworld.cpp and \ref engine/helloworld.cpp.

Now consider multiple connections. `proton::container` is easy to use
but restricted to a single thread. `proton::connection_engine` is not
thread-safe either, but *each engine is independent*. You can process
different connections in different threads, or use a thread pool to
process a dynamic set of connections.

The engine does not provide built-in polling and listening like the
`proton::container`, but you can drive engines using any polling or
threading strategy and any IO library (for example, epoll, kqueue,
solaris completion ports, IOCP proactor, boost::asio, libevent, etc.)
This can be important for optimizing performance or supporting diverse
platforms. The example \ref engine/broker.cpp shows a broker using
sockets and poll, but you can see how the code could be adapted.

`proton::connection_engine` also does not dictate the IO mechanism,
but it is an abstract class. `proton::socket::engine` provides
ready-made socket-based IO, but you can write your own subclass with
any IO code. Just override the `io_read`, `io_write` and `io_close`
methods. For example, the proton test suite implements an in-memory
connection using `std::deque` for test purposes.

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
