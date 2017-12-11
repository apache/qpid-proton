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

The **broker.rb** example application is a nice demonstration of doing something more interesting in Ruby with Proton.

The way the broker works is to listen to incoming connections, examine the components of the address for that connection, attach that connection to an exchange managing that address and then it sends any messages destined for that address to them.

The components of the broker example include:
 * **Broker** - A class that extends the MessagingHandler class. It accepts incoming connections, manages subscribing them to exchanges, and transfers messages between them.
 * **MessageQueue** - Distributes messages to subscriptions.

The Broker manages a map connecting a queue address to the instance of Exchange that holds references to the endpoints of interest.

The broker application demonstrates a new set of events:

 * **on_link_open** - Fired when a remote link is opened. From this event the broker grabs the address and subscribes the link to an exchange for that address.
 * **on_link_close** - Fired when a remote link is closed. From this event the broker grabs the address and unsubscribes the link from that exchange.
 * **on_connection_close** - Fired when a remote connection is closed but the local end is still open.
 * **on_transport_close** - Fired when the protocol transport has closed. The broker removes all links for the disconnected connection, avoiding workign with endpoints that are now gone.
