# C++ examples

Many of the examples expect a broker to be running on the standard AMQP
port. You can use any broker that supports AMQP 1.0, or you can use the simple
example `broker` provided here. Run the broker in a separate window before
running the other examples.

If you use another broker you will need to create a queue named `examples`.

## broker.cpp

A very simple "mini broker". You can use this to run other examples that reqiure
an intermediary, or you can use any AMQP 1.0 broker. This broker creates queues
automatically when a client tries to send or subscribe.

    $ ./broker
    broker listening on :5672

## helloworld.cpp

Basic example that connects to an intermediary on localhost:5672,
establishes a subscription from the 'examples' node on that
intermediary, then creates a sending link to the same node and sends
one message. On receving the message back via the subcription, the
connection is closed.

## helloworld_blocking.cpp

The same as the basic helloworld.cpp, but using a
synchronous/sequential style wrapper on top of the
asynchronous/reactive API. The purpose of this example is just to show
how different functionality can be easily layered should it be
desired.

## helloworld_direct.cpp

A variant of the basic helloworld example, that does not use an
intermediary, but listens for incoming connections itself. It
establishes a connection to itself with a link over which a single
message is sent. This demonstrates the ease with which a simple daemon
can be built using the API.

## simple_send.cpp

An example of sending a fixed number of messages and tracking their
(asynchronous) acknowledgement. Messages are sent through the 'examples' node on
an intermediary accessible on port 5672 on localhost.

# simple_recv.cpp

Subscribes to the 'examples' node on an intermediary accessible on port 5672 on
localhost. Simply prints out the body of received messages.

## encode_decode.cpp

Shows how C++ data types can be converted to and from AMQP types.
