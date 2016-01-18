// Examples overview.
//
// For a better overview, see the tutorial in the generated documentation.
// In your build directory do:
//     make docs-cpp
// then open proton-c/bindings/cpp/docs/html/tutorial.html in your browser.
//

// DEVELOPER NOTE: if you are adding or modifying examples you should keep this
// file and ../proton-c/bindings/cpp/docs/tutorial.hpp up to date.

/** \example helloworld.cpp

Basic example that connects to an intermediary on 127.0.0.1:5672,
establishes a subscription from the 'examples' nodeu on that
intermediary, then creates a sending link to the same node and sends
one message. On receving the message back via the subcription, the
connection is closed.

*/

/** \example helloworld_direct.cpp

A variant of the basic helloworld example, that does not use an
intermediary, but listens for incoming connections itself. It
establishes a connection to itself with a link over which a single
message is sent. This demonstrates the ease with which a simple daemon
can be built using the API.

*/

/** \example simple_send.cpp

An example of sending a fixed number of messages and tracking their
(asynchronous) acknowledgement. Messages are sent through the 'examples' node on
an intermediary accessible on 127.0.0.1:5672.

*/

/** \example simple_recv.cpp

Subscribes to the 'examples' node on an intermediary accessible
on 127.0.0.1:5672. Simply prints out the body of received messages.

*/

/** \example direct_send.cpp

Accepts an incoming connection and then sends like `simple_send`.  You can
connect directly to `direct_send` *without* a broker using \ref simple_recv.cpp.
Make sure to stop the broker first or use a different port for `direct_send`.

*/

/** \example direct_recv.cpp

Accepts an incoming connection and then receives like `simple_recv`.  You can
connect directly to `direct_recv` *without* a broker using \ref simple_send.cpp.
Make sure to stop the broker first or use a different port for `direct_recv`.

*/

/** \example encode_decode.cpp

Shows how C++ data types can be converted to and from AMQP types.

*/

/** \example client.cpp

The client part of a request-response example. Sends requests and
prints out responses. Requires an intermediary that supports the AMQP
1.0 dynamic nodes on which the responses are received. The requests
are sent through the 'examples' node.

*/

/** \example server.cpp

The server part of a request-response example, that receives requests
via the examples node, converts the body to uppercase and sends the
result back to the indicated reply address.

*/

/** \example server_direct.cpp

A variant of the server part of a request-response example, that
accepts incoming connections and does not need an intermediary. Much
like the original server, it receives incoming requests, converts the
body to uppercase and sends the result back to the indicated reply
address. Can be used in conjunction with any of the client
alternatives.

*/

** \example broker.hpp

Common logic for a simple "mini broker" that creates creates queues
automatically when a client tries to send or subscribe. This file contains
the `queue` class that queues messages and the `broker_handler` class
that manages queues and links and transfers messages to/from clients.

There are several broker programs all using the same logic but illustrating
different ways to run a server application.

*/

/** \example broker.cpp

A simple, single-threaded broker using the `proton::container`. You can use this
to run other examples that reqiure an intermediary, or you can use any AMQP 1.0
broker. This broker creates queues automatically when a client tries to send or
subscribe.

*/

/** \example select_broker.cpp

A simple, single-threaded, select-based broker using the `proton::engine`. This
broker implementation uses the standard `select` call as an illustration of
how to integrate proton with an external IO "framework", instead of letting
the `proton::container` manage IO.

*/
