## Simple Examples

### The Broker

The examples come with a sample broker which can be used by other examples and which also works as an example itself. For now we'll just start up the broker example and tell it to listen on port 8888:

````
$ ruby broker.rb amqp://:8888
Listening on amqp://:8888
````

This example broker will receive messages, create queues as needed, and deliver messages to endpoints.

### Hello World Using A Broker

Our first example creates an endpoint that sends messages to a queue to which it is subscribed. So it both sends and receives a message.

To start it, simply run:

```
$ ruby helloworld.rb //:8888
Hello world!
```

As you can see, the classic message was output by the example. Now let's take a look at what's going on under the covers.

#### Events When Talking To A Broker

The following events occur while **helloworld.rb** runs:

 * **on_start** - Fired when the application is started.
 * **on_sendable** - Fired when a message can be sent.
 * **on_message** - Fired when a message is received.

## More Complex Examples

Now that we've covered the basics with the archetypical hello world app, let's look at some more interesting examples.

The following two client examples send and receive messages to an external broker or server:

 * **simple_send.rb** - connect to a server, send messages to an address
 * **simple_recv.rb** - connect to a server, receives messages from an address

For example: start `broker.rb`; run `simple_send.rb` to send messages to a
broker queue; then `simple_recv.rb` to receive the messages from the broker.

The following two examples are *servers* that can be connected to directly, without a broker:

 * **direct_send.rb** - sends messages directly to a receiver and listens for responses itself, and
 * **direct_recv.rb** - receives messages directly.

For example if you start `direct_recv.rb`, you can connect to it directly with
`simple_send.rb` vice-versa with `direct_send.rb` and `simple_recv.rb`

In this set of examples we see the following event occurring, in addition to what we've seen before:

 * **on_transport_close** - Fired when the network transport is closed.

## Now About That Broker example

The **broker.rb** example application is a nice demonstration of doing something more interesting in Ruby with Proton, and shows how to use multiple threads.

The broker listens for incoming connections and sender/receiver links. It uses the source and target address of senders and receivers to identify a queue. Messages from receivers go on the queue, and are sent via senders.

The components of the broker example include:
 * **Broker** is a Listener::Handler that accepts connections, and manages the set of named queues.
 * **BrokerHandler** extends MessagingHandler to accept incoming connections, senders and receivers and transfers messages between them and the Broker's queues.
 * **MessageQueue** - A queue of messages that keeps track of waiting senders.

The broker application demonstrates a new set of events:

 * **on_sender_open** - Fired when a sender link is opened, the broker gets the address and starts sending messages from the corresponding queue.
 * **on_sender_close** - Fired when a sender link is closed, remove the sender from the queue so no more messages are sent.
 * **on_connection_close** - Fired when the remote connection is closes, close all senders.
 * **on_transport_close** - Fired when the transport (socket) has closed, close all senders.

It also demonstrates aspects of multi-threaded proton:

 * **Thread safe MessageQueue** Uses a Mutex to make actions atomic when called concurrently.

 * **Using WorkQueue** Proton objects like Sender are not thread safe.  They are
   normally only used in MessagingHandler#on_ callbacks.  To request work from a
   different thread you can add a code block to a WorkQueue, as shown in
   MessageQueue#push.

 * **Listener::Handler** The broker creates a new BrokerHandler instance for
   each accepted connection. The container ensures that calls on each handler instance
   are serialized even if there are multiple threads in the container.

 * **Calling Container#run in multiple threads** The Container uses threads that call
   #run as a thread pool to dispatch calls to MessagingHandler instances. Even
   if there are multiple threads, calls to handler instance are serialized.
