============
Hello World!
============

Tradition dictates that we start with hello world! However rather than
simply striving for the shortest program possible, we'll aim for a
more illustrative example while still restricting the functionality to
sending and receiving a single message.

.. literalinclude:: helloworld.py
   :lines: 21-
   :linenos:

This example uses proton in an event-driven or reactive manner. The
flow of control is an 'event loop', where the events may be triggered
by data arriving on a socket among other things and are then passed to
relevant 'handlers'. Applications are then structured as a set of
defined handlers for events of interest; to be notified of a
particular event, you define a class with an appropriately name method
on it, inform the event loop of that method which then calls it
whenever the event occurs.

The class we define in this example, ``HelloWorld``, has methods to
handle three types of events.

The first, ``on_connection_opened()``, is called when the connection
is opened, and when that occurs we create a receiver over which to
receive our message and a sender over which to send it.

The second method, ``on_credit()``, is called when our sender has been
issued by the peer with 'credit', allowing it to send messages. A
credit based flow control mechanism like this ensures we only send
messages when the recipient is ready and able to receive them. This is
particularly important when the volume of messages might be large. In
our case we are just going to send one message.

The third and final method, ``on_message()``, is called when a message
arrives. Within that method we simply print the body of the message
and then close the connection.

This particular example assumes a broker (or similar service), which
accepts connections and routes published messages to intended
recipients. The constructor for the ``HelloWorld`` class takes the
details of the broker to connect to, and the address through which the
message is sent and received (for a broker this corresponds to a queue
or topic name).

After an instance of ``HelloWorld`` is constructed, the event loop is
entered by the call to the ``run()`` method on the last line. This
call will return only when the loop determines there are no more
events possible (at which point our example program will then exit).

====================
Hello World, Direct!
====================

Though often used in conjunction with a broker, AMQP does not
*require* this. It also allows senders and receivers can communicate
directly if desired.

Let's modify our example to demonstrate this.

.. literalinclude:: helloworld_direct.py
   :lines: 21-
   :emphasize-lines: 17,33,38
   :linenos:

The first difference, on line 17, is that rather than creating a
receiver on the same connection as our sender, we listen for incoming
connections by invoking the ``listen() method on the ``EventLoop``
instance.

Another difference is that the ``EventLoop`` instance we use is not
the default instance as was used in the original example, but one we
construct ourselves on line 38, passing in some event handlers. The
first of these is ``HelloWorldReceiver``, as used in the original
example. We pass it to the event loop, because we aren't going to
directly create the receiver here ourselves. Rather we will accept an
incoming connection on which the message will be received. This
handler would then be notified of any incoming message event on any of
the connections the event loop controls. As well as our own handler, we
specify a couple of useful handlers from the ``proton_events``
toolkit. The ``Handshaker`` handler will ensure our server follows the
basic handshaking rules laid down by the protocol. The
``FlowController`` will issue credit for incoming messages. We won't
worry about them in more detail than that for now.

The last difference is that we close the ``acceptor`` returned from
the ``listen()`` call as part of the handling of the connection close
event (line 33).

So now we have our example working without a broker involved!

==========
The Basics
==========

TODO: These examples show reliable (at-least-once) send and receive
with reconnect ability. Need to write some explanation. Could also do
with some further cleanup.


.. literalinclude:: simple_recv.py
   :lines: 21-
   :linenos:

.. literalinclude:: simple_send.py
   :lines: 21-
   :linenos:

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

.. literalinclude:: server.py
   :lines: 21-
   :linenos:

The code here is not too different from the simple receiver example. When
we receive a request however, we look at the reply-to address and
create a sender for that over which to send the response. We'll cache
the senders incase we get further requests wit the same reply-to.

Now let's create a simple client to test this service out.

.. literalinclude:: client.py
   :lines: 21-
   :linenos:

As well as sending requests, we need to be able to get back the
responses. We create a receiver for that (see line 8), but we don't
specify an address, we set the dynamic option which tells the broker
we are connected to to create a temporary address over which we can
receive our responses.

We need to use the address allocated by the broker as the reply_to
address of our requests. To be notified when the broker has sent us
back the address to use, we add an ``on_link_remote_open()`` method to
our receiver's handler, and use that as the trigger to send our first
request.



