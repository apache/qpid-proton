########
Tutorial
########

============
Hello World!
============

Tradition dictates that we start with hello world! However rather than
simply striving for the shortest program possible, we'll aim for a
more illustrative example while still restricting the functionality to
sending and receiving a single message.

.. literalinclude:: ../../../../examples/python/helloworld.py
   :lines: 21-
   :linenos:

You can see the import of :py:class:`~proton.reactor.Container` from ``proton.reactor`` on the
second line. This is a class that makes programming with proton a
little easier for the common cases. It includes within it an event
loop, and programs written using this utility are generally structured
to react to various events. This reactive style is particularly suited
to messaging applications.

To be notified of a particular event, you define a class with the
appropriately name method on it. That method is then called by the
event loop when the event occurs.

We define a class here, ``HelloWorld``, which handles the key events of
interest in sending and receiving a message.

The ``on_start()`` method is called when the event loop first
starts. We handle that by establishing our connection (line 12), a
sender over which to send the message (line 13) and a receiver over
which to receive it back again (line 14).

The ``on_sendable()`` method is called when message can be transferred
over the associated sender link to the remote peer. We send out our
``Hello World!`` message (line 17), then close the sender (line 18) as
we only want to send one message. The closing of the sender will
prevent further calls to ``on_sendable()``.

The ``on_message()`` method is called when a message is
received. Within that we simply print the body of the message (line
21) and then close the connection (line 22).

Now that we have defined the logic for handling these events, we
create an instance of a :py:class:`~proton.reactor.Container`, pass it
our handler and then enter the event loop by calling
:py:meth:`~proton.reactor.Container.run()`. At this point control
passes to the container instance, which will make the appropriate
callbacks to any defined handlers.

To run the example you will need to have a broker (or similar)
accepting connections on that url either with a queue (or topic)
matching the given address or else configured to create such a queue
(or topic) dynamically. There is a simple broker.py script included
alongside the examples that can be used for this purpose if
desired. (It is also written using the API described here, and as such
gives an example of a slightly more involved application).

====================
Hello World, Direct!
====================

Though often used in conjunction with a broker, AMQP does not
*require* this. It also allows senders and receivers can communicate
directly if desired.

Let's modify our example to demonstrate this.

.. literalinclude:: ../../../../examples/python/helloworld_direct.py
   :lines: 21-
   :emphasize-lines: 11,21-22,24-25
   :linenos:

The first difference, on line 11, is that rather than creating a
receiver on the same connection as our sender, we listen for incoming
connections by invoking the
:py:meth:`~proton.reactor.Container.listen()` method on the
container.

As we only need then to initiate one link, the sender, we can do that
by passing in a url rather than an existing connection, and the
connection will also be automatically established for us.

We send the message in response to the ``on_sendable()`` callback and
print the message out in response to the ``on_message()`` callback
exactly as before.

However we also handle two new events. We now close the connection
from the senders side once the message has been accepted (line
22). The acceptance of the message is an indication of successful
transfer to the peer. We are notified of that event through the
``on_accepted()`` callback. Then, once the connection has been closed,
of which we are notified through the ``on_closed()`` callback, we stop
accepting incoming connections (line 25) at which point there is no
work to be done and the event loop exits, and the run() method will
return.

So now we have our example working without a broker involved!

=============================
Asynchronous Send and Receive
=============================

Of course, these ``HelloWorld!`` examples are very artificial,
communicating as they do over a network connection but with the same
process. A more realistic example involves communication between
separate processes (which could indeed be running on completely
separate machines).

Let's separate the sender from the receiver, and let's transfer more than
a single message between them.

We'll start with a simple sender.

.. literalinclude:: ../../../../examples/python/simple_send.py
   :lines: 21-
   :linenos:

As with the previous example, we define the application logic in a
class that handles various events. As before, we use the
``on_start()`` event to establish our sender link over which we will
transfer messages and the ``on_sendable()`` event to know when we can
transfer our messages.

Because we are transferring more than one message, we need to keep
track of how many we have sent. We'll use a ``sent`` member variable
for that. The ``total`` member variable will hold the number of
messages we want to send.

AMQP defines a credit-based flow control mechanism. Flow control
allows the receiver to control how many messages it is prepared to
receive at a given time and thus prevents any component being
overwhelmed by the number of messages it is sent.

In the ``on_sendable()`` callback, we check that our sender has credit
before sending messages. We also check that we haven't already sent
the required number of messages.

The ``send()`` call on line 20 is of course asynchronous. When it
returns the message has not yet actually been transferred across the
network to the receiver. By handling the ``on_accepted()`` event, we
can get notified when the receiver has received and accepted the
message. In our example we use this event to track the confirmation of
the messages we have sent. We only close the connection and exit when
the receiver has received all the messages we wanted to send.

If we are disconnected after a message is sent and before it has been
confirmed by the receiver, it is said to be ``in doubt``. We don't
know whether or not it was received. In this example, we will handle
that by resending any in-doubt messages. This is known as an
'at-least-once' guarantee, since each message should eventually be
received at least once, though a given message may be received more
than once (i.e. duplicates are possible). In the ``on_disconnected()``
callback, we reset the sent count to reflect only those that have been
confirmed. The library will automatically try to reconnect for us, and
when our sender is sendable again, we can restart from the point we
know the receiver got to.

Now let's look at the corresponding receiver:

.. literalinclude:: ../../../../examples/python/simple_recv.py
   :lines: 21-
   :linenos:

Here we handle the ``on_start()`` by creating our receiver, much like
we did for the sender. We also handle the ``on_message()`` event for
received messages and print the message out as in the ``Hello World!``
examples. However we add some logic to allow the receiver to wait for
a given number of messages, then to close the connection and exit. We
also add some logic to check for and ignore duplicates, using a simple
sequential id scheme.

Again, though sending between these two examples requires some sort of
intermediary process (e.g. a broker), AMQP allows us to send messages
directly between two processes without this if we so wish. In that
case one or other of the processes needs to accept incoming socket
connections. Let's create a modified version of the receiving example
that does this:

.. literalinclude:: ../../../../examples/python/direct_recv.py
   :lines: 21-
   :emphasize-lines: 13,25
   :linenos:

There are only two differences here. On line 13, instead of initiating
a link (and implicitly a connection), we listen for incoming
connections. On line 25, when we have received all the expected
messages, we then stop listening for incoming connections by closing
the acceptor object.

You can use the original send example now to send to this receiver
directly. (Note: you will need to stop any broker that is listening on
the 5672 port, or else change the port used by specifying a different
address to each example via the -a command line switch).

We could equally well modify the original sender to allow the original
receiver to connect to it. Again that just requires two modifications:

.. literalinclude:: ../../../../examples/python/direct_send.py
   :lines: 21-
   :emphasize-lines: 15,28
   :linenos:

As with the modified receiver, instead of initiating establishment of
a link, we listen for incoming connections on line 15 and then on line
28, when we have received confirmation of all the messages we sent, we
can close the acceptor in order to exit. The symmetry in the
underlying AMQP that enables this is quite unique and elegant, and in
reflecting this the proton API provides a flexible toolkit for
implementing all sorts of interesting intermediaries (the broker.py
script provided as a simple broker for testing purposes provides an
example of this).

To try this modified sender, run the original receiver against it.

================
Request/Response
================

A common pattern is to send a request message and expect a response
message in return. AMQP has special support for this pattern. Let's
have a look at a simple example. We'll start with the 'server',
i.e. the program that will process the request and send the
response. Note that we are still using a broker in this example.

Our server will provide a very simple service: it will respond with
the body of the request converted to uppercase.

.. literalinclude:: ../../../../examples/python/server.py
   :lines: 21-
   :linenos:

The code here is not too different from the simple receiver
example. When we receive a request however, we look at the
:py:attr:`~proton.Message.reply_to` address on the
:py:class:`~proton.Message` and create a sender for that over which to
send the response. We'll cache the senders incase we get further
requests with the same reply_to.

Now let's create a simple client to test this service out.

.. literalinclude:: ../../../../examples/python/client.py
   :lines: 21-
   :linenos:

As well as sending requests, we need to be able to get back the
responses. We create a receiver for that (see line 14), but we don't
specify an address, we set the dynamic option which tells the broker
we are connected to to create a temporary address over which we can
receive our responses.

We need to use the address allocated by the broker as the reply_to
address of our requests, so we can't send them until the broker has
confirmed our receiving link has been set up (at which point we will
have our allocated address). To do that, we add an
``on_link_opened()`` method to our handler class, and if the link
associated with event is the receiver, we use that as the trigger to
send our first request.

Again, we could avoid having any intermediary process here if we
wished. The following code implementas a server to which the client
above could connect directly without any need for a broker or similar.

.. literalinclude:: ../../../../examples/python/server_direct.py
   :lines: 21-
   :linenos:

Though this requires some more extensive changes than the simple
sending and receiving examples, the essence of the program is still
the same. Here though, rather than the server establishing a link for
the response, it relies on the link that the client established, since
that now comes in directly to the server process.

Miscellaneous
=============

Many brokers offer the ability to consume messages based on a
'selector' that defines which messages are of interest based on
particular values of the headers. The following example shows how that
can be achieved:

.. literalinclude:: ../../../../examples/python/selected_recv.py
   :lines: 21-
   :emphasize-lines: 10
   :linenos:

When creating the receiver, we specify a Selector object as an
option. The options argument can take a single object or a
list. Another option that is sometimes of interest when using a broker
is the ability to 'browse' the messages on a queue, rather than
consumig them. This is done in AMQP by specifying a distribution mode
of 'copy' (instead of 'move' which is the expected default for
queues). An example of that is shown next:

.. literalinclude:: ../../../../examples/python/queue_browser.py
   :lines: 21-
   :emphasize-lines: 10
   :linenos:
