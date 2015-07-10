C++ examples
============

Many of the examples expect a broker to be running on the standard AMQP
port. You can use any broker that supports AMQP 1.0, or you can use the simple
example `broker` provided here. Run the broker in a separate window before
running the other examples.

If you use another broker you will need to create a queue named `examples`.

NOTE: Most of these examples are described in more detail as part of the \ref tutorial.

Note on Brokers and URLs
------------------------

Some of the examples require an AMQP *broker* that can receive, store and send messages.

There is a very simple broker included as an example (\ref broker.cpp) which you can use.
Run it like this:

    broker [URL]

The default URL is "0.0.0.0:5672" which listens on the standard AMQP port on all
network interfaces.

Instead of the example broker, you can use any AMQP 1.0 compliant broker. You
must configure your broker to have a queue or topic named "examples".

Most of the examples take an optional URL argument, the simplest form is:

    HOST:PORT/ADDRESS

It usually defaults to `127.0.0.1:5672/examples`, but you can change this if
your broker is on a different host or port, or you want to use a different queue
or topic name (the ADDRESS part of the URL). URL details are at `proton::url`

broker.cpp
----------

A very simple "mini broker". You can use this to run other examples that reqiure
an intermediary, or you can use any AMQP 1.0 broker. This broker creates queues
automatically when a client tries to send or subscribe.

Source: \ref broker.cpp

helloworld.cpp
--------------

Basic example that connects to an intermediary on 127.0.0.1:5672,
establishes a subscription from the 'examples' nodeu on that
intermediary, then creates a sending link to the same node and sends
one message. On receving the message back via the subcription, the
connection is closed.

Source: \ref helloworld.cpp

helloworld_direct.cpp
---------------------

A variant of the basic helloworld example, that does not use an
intermediary, but listens for incoming connections itself. It
establishes a connection to itself with a link over which a single
message is sent. This demonstrates the ease with which a simple daemon
can be built using the API.

Source: \ref helloworld_direct.cpp

helloworld_blocking.cpp
-----------------------

The same as the basic helloworld.cpp, but using a
synchronous/sequential style wrapper on top of the
asynchronous/reactive API. The purpose of this example is just to show
how different functionality can be easily layered should it be
desired.

Source: \ref helloworld_blocking.cpp

simple_send.cpp
---------------

An example of sending a fixed number of messages and tracking their
(asynchronous) acknowledgement. Messages are sent through the 'examples' node on
an intermediary accessible on 127.0.0.1:5672.

Source: \ref simple_send.cpp

simple_recv.cpp
---------------

Subscribes to the 'examples' node on an intermediary accessible
on 127.0.0.1:5672. Simply prints out the body of received messages.

Source: \ref simple_recv.cpp

direct_send.cpp
---------------

Accepts an incoming connection and then sends like `simple_send`.  You can
connect directly to `direct_send` *without* a broker using \ref simple_recv.cpp.
Make sure to stop the broker first or use a different port for `direct_send`.

Source: \ref direct_send.cpp

direct_recv.cpp
---------------

Accepts an incoming connection and then receives like `simple_recv`.  You can
connect directly to `direct_recv` *without* a broker using \ref simple_send.cpp.
Make sure to stop the broker first or use a different port for `direct_recv`.

Source: \ref direct_recv.cpp

encode_decode.cpp
-----------------

Shows how C++ data types can be converted to and from AMQP types.

Source: \ref encode_decode.cpp

    <!-- For generated doxygen docmentation. Note \ref tags above are also for doxygen -->
    \example helloworld.cpp
    \example helloworld_direct.cpp
    \example helloworld_blocking.cpp
    \example broker.cpp
    \example encode_decode.cpp
    \example simple_recv.cpp
    \example simple_send.cpp
    \example direct_recv.cpp
    \example direct_send.cpp
