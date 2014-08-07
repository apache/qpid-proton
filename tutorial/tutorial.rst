============
Hello World!
============

Let's start, in time honoured tradition, with hello world!:

.. literalinclude:: helloworld.py
   :lines: 21-
   :linenos:

You can see the import of ``Runtime`` from ``proton_utils`` on the
second line. This is a helper class that makes programming with proton
a little easier for the common cases.

We use the ``Runtime`` on line 12. Specifically we use a special
default instance of it. We'll see some examples using other instances
later. Line 12 uses the runtime to make a connection to the desired
host and port via the ``connect()`` call. This call returns a
``MessagingContext`` object through which we can create objects for
sending and receiving messages to the process it is connected to.

On line 13 we create a receiver through which to receiver messages
from the specified address. We specify a ``handler`` parameter, with
an instance of our ``HelloWorld`` class as it's value. The ``handler``
parameter provides a way of being notified of important events related
to the receiver being created. The event we care about most is the
receiving of a message. To be notified of that we define a
``received`` method on our handler which will be called whenever a
message for that receiver arrives.  As well as the received message,
this method also gets passed the receiver over which the message
arrived and a ``delivery`` handle associated with it, which we can
ignore for now.  In our example we simply print the body of the
message, then close the connection of the receiver it arrived on.

Now we are all ready to receive and print our message. All we need to
do is send one! To do so we use the ``MessagingContext`` object to
create a sender for the same address we used when creating the
receiver, and then we send a message over it.

Finally we allow the runtime to process these instructions and handle
all the necessary IO by calling ``run()`` on it in line 15.

To run this example as it is, you need to have an AMQP broker running
locally on port 5672, with a queue (or topic) named ``examples``, or
configured to create that dynamically. The broker must also allow
unauthenticated connections.

In fact, if your broker doesn't have the requisite queue, the example
just hangs. Let's modify the example to handle that a little more
gracefully.

.. literalinclude:: helloworld_2.py
   :lines: 21-
   :emphasize-lines: 12-15
   :linenos:

All we have added is a new method to our receiver's handler. This
method is called ``closed()`` and it is called whenever the remote
process closes our receiver. We'll print any error if specified and
then close the connection. If you now run it against a broker that
doesn't have (and will not automatically create) a queue named
``examples`` then it should exit with a more informative error
message. This demonstrates a key concept in using proton, namely that
you often structure your logic to react to particular events.

====================
Hello World, Direct!
====================

Though often used in conjunction with a broker, AMQP does not
*require* this. It also allows senders and receivers can communicate
directly if desired.

Let's modify our example to demonstrate this.

.. literalinclude:: helloworld_3.py
   :lines: 21-
   :emphasize-lines: 12-14
   :linenos:

The first difference, on line 12, is that we create our own
``Runtime`` instance rather than just using the default instance. We
pass in some handler objects. The first of these is our ``HelloWorld``
handler as used in the original example. We pass it to the runtime,
because we aren't going to directly create the receiver here
ourselves. Rather we will accept an incoming connection on which the
message will be received. As well as our own handler, we specify a
couple of useful handlers from the ``proton_utils`` toolkit. The
``Handshaker`` handler will ensure our server follows the basic
handshaking rules laid down by the protocol. The ``FlowController``
will issue credit for incoming messages. We won't worry about them in
more detail than that for now.

On line 13 we then invoke ``listen()`` on our runtime. This starts a
server socket listening for incoming connections on the specified
interface and port. Then on line 14 we use ``connect`` as before on
our runtime instance to establish an outgoing connection back to
ourselves. As before we create a sender on this connection and send
our message over it. So now we have our example working without a
broker involved!

However, the example doesn't exit after the message is printed. This
is because we are still listenting for incoming connections; the
runtime is still running. Let's now change it to shutdown cleanly when
done.

.. literalinclude:: helloworld_4.py
   :lines: 21-
   :emphasize-lines: 12-17,20,21
   :linenos:

On line 21 we pass a handler to the ``connect()`` call on our
runtime. This is similar to what we did when creating a receiver in
the original example. Here however the handler is scoped to the
connection. We are interested in reacting to the closing of the
connection by the remote peer by closing the server socket we have
listening for incoming connections. The call to ``listen()`` returns
an object we can ``close()`` to accomplish this, so we modify line 20
to create an object to use as our connection scoped handler, passing
in this reference to the incoming socket acceptor. Now the ``run()``
call returns when we are finished and the example exits cleanly.

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
the sender we create with an ``accepted()`` method defined on it. This
will be called whenever a message sent by the sender is accepted by
the remote peer.

When sending a large number of messages, we need to consider whether
the remote peer is able to handle them all. AMQP has a powerful flow
control mechanism through which processes can limit the incoming flow
of messages. If we implement a ``link_flow()`` method on our sender's
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
back the address to use, we add an ``opened()`` method to our
receiver's handler, and use that as the trigger to send our first
request.



