// Examples overview.
//
// For a better overview, see the tutorial in the generated documentation.
//
// In your build directory do:
//
//     make docs-cpp
//
// then open proton-c/bindings/cpp/docs/html/tutorial.html in your browser.

// DEVELOPER NOTE: if you are adding or modifying examples you should keep this
// file and ../proton-c/bindings/cpp/docs/tutorial.hpp up to date.

/** @example helloworld.cpp

Connects to a broker on 127.0.0.1:5672, establishes a subscription
from the 'examples' node, and creates a sending link to the same
node. Sends one message and receives it back.

*/

/** @example helloworld_direct.cpp

Variation of helloworld that does not use a broker, but listens for
incoming connections itself. It establishes a connection to itself
with a link over which a single message is sent. This demonstrates the
ease with which a simple daemon an be built using the API.

*/

/** @example simple_send.cpp

An example of sending a fixed number of messages and tracking their
(asynchronous) acknowledgement. Messages are sent through the 'examples' node on
an intermediary accessible on 127.0.0.1:5672.

*/

/** @example simple_recv.cpp

Subscribes to the 'examples' node on an intermediary accessible
on 127.0.0.1:5672. Simply prints out the body of received messages.

*/

/** @example direct_send.cpp

Accepts an incoming connection and then sends like `simple_send`.  You can
connect directly to `direct_send` *without* a broker using \ref simple_recv.cpp.
Make sure to stop the broker first or use a different port for `direct_send`.

*/

/** @example direct_recv.cpp

Accepts an incoming connection and then receives like `simple_recv`.  You can
connect directly to `direct_recv` *without* a broker using \ref simple_send.cpp.
Make sure to stop the broker first or use a different port for `direct_recv`.

*/

/// @cond INTERNAL
/** @example encode_decode.cpp

Shows how C++ data types can be converted to and from AMQP types.

*/
/// @endcond

/** @example client.cpp

The client part of a request-response example. Sends requests and
prints out responses. Requires an intermediary that supports the AMQP
1.0 dynamic nodes on which the responses are received. The requests
are sent through the 'examples' node.

*/

/** @example server.cpp

The server part of a request-response example, that receives requests
via the examples node, converts the body to uppercase and sends the
result back to the indicated reply address.

*/

/** @example server_direct.cpp

A variant of the server part of a request-response example that
accepts incoming connections and does not need an intermediary. Much
like the original server, it receives incoming requests, converts the
body to uppercase and sends the result back to the indicated reply
address. Can be used in conjunction with any of the client
alternatives.

*/

/** @example recurring_timer.cpp

Shows how to implement recurring time-based events using the scheduler. 

*/

/** @example broker.hpp

Common logic for a simple "mini broker" that creates creates queues
automatically when a client tries to send or subscribe. This file contains
the `queue` class that queues messages and the `broker_handler` class
that manages queues and links and transfers messages to/from clients.

Examples \ref broker.cpp and \ref engine/broker.cpp use this same
broker logic but show different ways to run it in a server application.

*/

/** @example broker.cpp

A simple, single-threaded broker using the `proton::container`. You can use this
to run other examples that reqiure an intermediary, or you can use any AMQP 1.0
broker. This broker creates queues automatically when a client tries to send or
subscribe.

Uses the broker logic from \ref broker.hpp, the same logic as the
`proton::connection_engine` broker example \ref engine/broker.cpp.

*/

//////////////// connection_engine examples.

/** \example engine/helloworld.cpp

`proton::connection_engine` example to send a "Hello World" message to
itself. Compare with the corresponding `proton::container` example \ref
helloworld.cpp.

*/

/** \example engine/simple_send.cpp

`proton::connection_engine` example of sending a fixed number of messages and
tracking their (asynchronous) acknowledgement. Messages are sent through the
'examples' node on an intermediary accessible on 127.0.0.1:5672.

*/

/** \example engine/simple_recv.cpp

`proton::connection_engine` example that subscribes to the 'examples' node and prints
 the body of received messages.

*/

/** \example engine/direct_send.cpp

`proton::connection_engine` example accepts an incoming connection and then
sends like `simple_send`.  You can connect directly to `direct_send` *without* a
broker using \ref simple_recv.cpp.  Make sure to stop the broker first or use a
different port for `direct_send`.

*/

/** \example engine/direct_recv.cpp

`proton::connection_engine` example accepts an incoming connection and then
receives like `simple_recv`.  You can connect directly to `direct_recv`
*without* a broker using \ref simple_send.cpp.  Make sure to stop the broker
first or use a different port for `direct_recv`.

*/

/** \example engine/client.cpp

`proton::connection_engine` client for request-response example. Sends requests and
prints out responses. Requires an intermediary that supports the AMQP 1.0
dynamic nodes on which the responses are received. The requests are sent through
the 'examples' node.

*/

/** \example engine/server.cpp

`proton::connection_engine` server for request-response example, that receives
requests via the examples node, converts the body to uppercase and sends the
result back to the indicated reply address.

*/

/** \example engine/broker.cpp

A simple, single-threaded broker using the `proton::container`. You can use this
to run other examples that reqiure an intermediary, or you can use any AMQP 1.0
broker. This broker creates queues automatically when a client tries to send or
subscribe.

Uses the broker logic from \ref broker.hpp, the same logic as the
proton::container` broker example \ref broker.cpp.

*/
