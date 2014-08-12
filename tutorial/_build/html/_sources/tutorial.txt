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

You can see the import of ``EventLoop`` from ``proton_events`` on the
second line. This is a helper class that makes programming with proton
a little easier for the common cases. It includes within it an event
loop, and programs written using this utility are generally structured
to react to various events. This reactive style is particularly suited
to messaging applications.

To be notified of a particular event, you define a class with the
appropriately name method on it. That method is then called by the
event loop when the event occurs.

The first class we define, ``HelloWorldReceiver``, handles the event
where a message is received and so implements a ``on_message()``
method. Within that we simply print the body of the message (line 6)
and then close the connection (line 7).

The second class, ``HelloWorldSender``, handles the event where the
flow of messages is enabled over our sending link by implementing a
``on_link_flow()`` method and sending the message within that. Doing
this ensures that we only send when the recipient is ready and able to
receive the message. This is particularly important when the volume of
messages might be large. In our case we are just going to send one
message, which we do on line 11, so we can then just close the sending
link on line 12.

The ``HelloWorld`` class ties everything together. It's constructor
takes the instance of the event loop to use, a url to connect to, and
an address through which the message will be sent. To run the example
you will need to have a broker (or similar) accepting connections on
that url either with a queue (or topic) matching the given address or
else configured to create such a queue (or topic) dynamically.

On line 17 we request that a connection be made to the process this
url refers to by calling ``connect()`` on the ``EventLoop``. This call
returns a ``MessagingContext`` object through which we can create
objects for sending and receiving messages to the process it is
connected to. However we will delay doing that until our connection is
fully established, i.e. until the remote peer 'opens' the connection
(the open here is the 'handshake' for establishing an operational AMQP
connection).

To be notified of this we pass a reference to self as the handler in
``connect()`` and define an ``on_connection_remote_open()`` method
within which we can create our receiver and sender using the
connection context we obtained from the earlier ``connect()`` call,
and passing the handler implementations defined by
``HelloWorldReceiver`` and ``HelloWorldSender`` respectively.

We'll add definitions to ``HelloWorld`` of ``on_link_remote_close()``
and ``on_connection_remote_close()`` also, so that we can be notified
if the broker we are connected to closes either link or the connection
for any reason.

Finally we actually enter the event loop, to handle all the necessary
IO and make all the necessary event callbacks, by calling ``run()`` on
it.

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

So much for hello world! Let's explore a little more. Separating out
the receiving logic and receiving messages until the program is
stopped, we get the following example (which has the same broker
requirements mentioned for the first hello world example).

.. literalinclude:: simple_recv.py
   :lines: 21-
   :linenos:

Often we want to be notified whether the messages we send arrive at
their intended destination. We can do that by specifying a handler for
the sender we create with an ``on_message()`` method defined on it. This
will be called whenever a message sent by the sender is accepted by
the remote peer.

When sending a large number of messages, we need to consider whether
the remote peer is able to handle them all. AMQP has a powerful flow
control mechanism through which processes can limit the incoming flow
of messages. If we implement a ``on_link_flow()`` method on our sender's
handler, this will be called whenever the sender is allowed to send
and will prevent messages building up due to the receivers inability
to process them.

Separating out the sending logic, extending it to send a given number
of messages and incorporating the two handler methods just described
we get:

.. literalinclude:: simple_send.py
   :lines: 21-
   :linenos:

============
Reconnecting
============

TODO: This shows a basic reconnect for the receiver. Need some backoff
logic which requires some sort of support for timers in the event
loop.

.. literalinclude:: simple_recv_2.py
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



