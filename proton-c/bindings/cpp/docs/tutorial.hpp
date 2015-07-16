// -*-markdown-*-
// NOTE: doxygen can include markdown pages directly but there seems to be a bug
// that shows messed-up line numbers in \skip \until code extracts so this file
// is markdown wrapped in a C++ comment - which works.

/** \page tutorial Tutorial
Tutorial 
========

This is a brief tutorial that will walk you through the fundamentals of building
messaging applications in incremental steps. There are further examples, in
addition the ones mentioned in the tutorial.

Some of the examples require an AMQP *broker* that can receive, store and send
messages. \ref broker.cpp is a simple example broker which you can use. When run
without arguments it listens on `0.0.0.0:5672`, the standard AMQP port on all
network interfaces. To use a different port or network interface:

    broker -a <host>:<port>

Instead of the example broker, you can use any AMQP 1.0 compliant broker. You
must configure your broker to have a queue (or topic) named "examples".

Most of the examples take an optional URL argument or `-a URL` option the
URL looks like:

    HOST:PORT/ADDRESS

It usually defaults to `127.0.0.1:5672/examples`, but you can change this if
your broker is on a different host or port, or you want to use a different queue
or topic name (the ADDRESS part of the URL). URL details are at `proton::url`

Hello World!
------------

\dontinclude helloworld.cpp

Tradition dictates that we start with hello world! This example sends a message
to a broker and the receives the same message back to demonstrate sending and
receiving. In a realistic system the sender and receiver would normally be in
different processes. The complete example is \ref helloworld.cpp

We will include the following classes: `proton::container` runs an event loop
which dispatches events to a `proton::messaging_handler`. This allows a
*reactive* style of programming which is well suited to messaging
applications. `proton::url` is a simple parser for the URL format described
above.

\skip   proton/container
\until  proton/url

We will define a class `hello_world` which is a subclass of
`proton::messaging_handler` and over-rides functions to handle the events
of interest in sending and receiving a message.

\skip class hello_world
\until {}

`on_start()` is called when the event loop first starts. We handle that by
establishing a connection and creating a sender and a receiver.

\skip on_start
\until }

`on_sendable()` is called when message can be transferred over the associated
sender link to the remote peer. We create a `proton::message`, set the message
body to `"Hello World!"` and send the message. Then we close the sender as we only
want to send one message. Closing the sender will prevent further calls to
`on_sendable()`.

\skip on_sendable
\until }

`on_message()` is called when a message is received. We just print the body of
the message and close the connection, as we only want one message

\skip on_message
\until }

The message body is a `proton::value`, see the documentation for more on how to
extract the message body as type-safe C++ values.

Our `main` function creates an instance of the `hello_world` handler and a
proton::container using that handler. Calling `proton::container::run` sets
things in motion and returns when we close the connection as there is nothing
further to do. It may throw an exception, which will be a subclass of
`proton::error`. That in turn is a subclass of `std::exception`.

\skip main
\until }
\until }
\until }

Hello World, Direct!
--------------------

\dontinclude helloworld_direct.cpp

Though often used in conjunction with a broker, AMQP does not *require* this. It
also allows senders and receivers can communicate directly if desired.

We will modify our example to send a message directly to itself. This is a bit
contrived but illustrates both sides of the direct send/receive scenario. Full
code at \ref helloworld_direct.cpp

The first difference, is that rather than creating a receiver on the same
connection as our sender, we listen for incoming connections by invoking the
`proton::container::listen()` method on the container.

\skip on_start
\until }

As we only need then to initiate one link, the sender, we can do that by
passing in a url rather than an existing connection, and the connection
will also be automatically established for us.

We send the message in response to the `on_sendable()` callback and
print the message out in response to the `on_message()` callback exactly
as before.

\skip on_sendable
\until }
\until }

However we also handle two new events. We now close the connection from
the senders side once the message has been accepted.
The acceptance of the message is an indication of successful transfer to the
peer. We are notified of that event through the `on_accepted()`
callback.

\skip on_accepted
\until }

Then, once the connection has been closed, of which we are
notified through the `on_closed()` callback, we stop accepting incoming
connections at which point there is no work to be done and the
event loop exits, and the run() method will return.

\skip on_closed
\until }

So now we have our example working without a broker involved!

Note that for this example we paick an "unusual" port 8888 since we are talking
to ourselves rather than a broker.

\skipline url =

Asynchronous Send and Receive
-----------------------------

Of course, these `HelloWorld!` examples are very artificial, communicating as
they do over a network connection but with the same process. A more realistic
example involves communication between separate processes (which could indeed be
running on completely separate machines).

Let's separate the sender from the receiver, and transfer more than a single
message between them.

We'll start with a simple sender \ref simple_send.cpp.

\dontinclude simple_send.cpp

As with the previous example, we define the application logic in a class that
handles events. Because we are transferring more than one message, we need to
keep track of how many we have sent. We'll use a `sent` member variable for
that.  The `total` member variable will hold the number of messages we want to
send.

\skip class simple_send
\until total

As before, we use the `on_start()` event to establish our sender link over which
we will transfer messages.

\skip on_start
\until }

AMQP defines a credit-based flow control mechanism. Flow control allows
the receiver to control how many messages it is prepared to receive at a
given time and thus prevents any component being overwhelmed by the
number of messages it is sent.

In the `on_sendable()` callback, we check that our sender has credit
before sending messages. We also check that we haven't already sent the
required number of messages.

\skip on_sendable
\until }
\until }

The `proton::sender::send()` call above is asynchronous. When it returns the
message has not yet actually been transferred across the network to the
receiver. By handling the `on_accepted()` event, we can get notified when the
receiver has received and accepted the message. In our example we use this event
to track the confirmation of the messages we have sent. We only close the
connection and exit when the receiver has received all the messages we wanted to
send.

\skip on_accepted
\until }
\until }

If we are disconnected after a message is sent and before it has been
confirmed by the receiver, it is said to be `in doubt`. We don't know
whether or not it was received. In this example, we will handle that by
resending any in-doubt messages. This is known as an 'at-least-once'
guarantee, since each message should eventually be received at least
once, though a given message may be received more than once (i.e.
duplicates are possible). In the `on_disconnected()` callback, we reset
the sent count to reflect only those that have been confirmed. The
library will automatically try to reconnect for us, and when our sender
is sendable again, we can restart from the point we know the receiver
got to.

\skip on_disconnected
\until }

\dontinclude simple_recv.cpp

Now let's look at the corresponding receiver \ref simple_recv.cpp

This time we'll use an `expected` member variable for for the number of messages we expecct and
a `received` variable to count how many we have received so far.send.

\skip class simple_recv
\until received

We handle `on_start()` by creating our receiver, much like we
did for the sender.

\skip on_start
\until }

We also handle the `on_message()` event for received messages and print the
message out as in the `Hello World!` examples.  However we add some logic to
allow the receiver to wait for a given number of messages, then to close the
connection and exit. We also add some logic to check for and ignore duplicates,
using a simple sequential id scheme.

\skip on_message
\until }

Direct Send and Receive
-----------------------

Sending between these two examples requires an intermediary broker since neither
accepts incoming connections. AMQP allows us to send messages directly between
two processes. In that case one or other of the processes needs to accept
incoming connections. Let's create a modified version of the receiving example
that does this with \ref direct_recv.cpp

\dontinclude direct_recv.cpp

There are only two differences here. Instead of initiating a link (and
implicitly a connection), we listen for incoming connections.


\skip on_start
\until }

When we have received all the expected messages, we then stop listening for
incoming connections by closing the acceptor object.

\skip on_message
\until }
\until }
\until }
\until }

You can use the \ref simple_send.cpp example to send to this receiver
directly. (Note: you will need to stop any broker that is listening on the 5672
port, or else change the port used by specifying a different address to each
example via the -a command line switch).

We can also modify the sender to allow the original receiver to connect to it,
in \ref direct_send.cpp. Again that just requires two modifications:

\dontinclude direct_send.cpp

As with the modified receiver, instead of initiating establishment of a
link, we listen for incoming connections.

\skip on_start
\until }

When we have received confirmation of all the messages we sent, we can
close the acceptor in order to exit.

\skip on_accepted
\until }
\until }

To try this modified sender, run the original \ref simple_recv.cpp against it.

The symmetry in the underlying AMQP that enables this is quite unique and
elegant, and in reflecting this the proton API provides a flexible toolkit for
implementing all sorts of interesting intermediaries (\ref broker.cpp provided
as a simple broker for testing purposes is an example of this).

Request/Response
----------------

\todo TODO missing example in C++

A common pattern is to send a request message and expect a response message in
return. AMQP has special support for this pattern. Let's have a look at a simple
example. We'll start with \ref server.cpp, the program that will process the
request and send the response. Note that we are still using a broker in this
example.

\todo TODO insert server snips

Our server will provide a very simple service: it will respond with the
body of the request converted to uppercase.

The code here is not too different from the simple receiver example.  When we
receive a request however, we look at the proton::message::reply_to address on
proton::message and create a sender for that over which to send the
response. We'll cache the senders incase we get further requests with the same
reply\_to.

Now let's create a simple \ref client.cpp to test this service out.

\todo TODO insert client snips

As well as sending requests, we need to be able to get back the
responses. We create a receiver for that (see line 14), but we don't
specify an address, we set the dynamic option which tells the broker we
are connected to to create a temporary address over which we can receive
our responses.

We need to use the address allocated by the broker as the reply\_to
address of our requests, so we can't send them until the broker has
confirmed our receiving link has been set up (at which point we will
have our allocated address). To do that, we add an `on_link_opened()`
method to our handler class, and if the link associated with event is
the receiver, we use that as the trigger to send our first request.

Again, we could avoid having any intermediary process here if we wished.
The following code implementas a server to which the client above could
connect directly without any need for a broker or similar.

\todo TODO missing server_direct.cpp

Though this requires some more extensive changes than the simple sending
and receiving examples, the essence of the program is still the same.
Here though, rather than the server establishing a link for the
response, it relies on the link that the client established, since that
now comes in directly to the server process.

### Miscellaneous

Many brokers offer the ability to consume messages based on a 'selector'
that defines which messages are of interest based on particular values
of the headers. The following example shows how that can be achieved:

\todo TODO selected_recv.cpp

When creating the receiver, we specify a Selector object as an option.
The options argument can take a single object or a list. Another option
that is sometimes of interest when using a broker is the ability to
'browse' the messages on a queue, rather than consumig them. This is
done in AMQP by specifying a distribution mode of 'copy' (instead of
'move' which is the expected default for queues). An example of that is
shown next:

\todo TODO queue_browser.cpp
*/
