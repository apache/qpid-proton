## What Is The Reactor?

A little outside of the scope of this document, but the reactor is an event source for letting an application know about events in the Proton messaging system. With this set of APIs an application can be register handlers that are notified when a connection is created, a message received, or a session closes.

### Handlers

An application creates **handlers**, objects which provide methods through which the reactor notifies the application's components of events and allows them each to handle the ones in which they are interested (see the Chain Of Responsibility design pattern for more on this idea). There are some pre-defined handlers for responding to incoming message events, outgoing message events, data flow and managing the AMQP endpoints. Look in the **Qpid::Proton::Handlers** package for more details on these classes.

## Simple Reactor Examples

### The Broker

The reactor examples come with a sample broker which can be used by other examples and which also works as an example itself. For now we'll just start up the broker example and tell it to listen on port 8888:

````
$ ruby ../examples/ruby/reactor/broker.rb  --address=0.0.0.0:8888
Listening on 0.0.0.0:8888
````

This example broker will receive messages, create queues as needed, and deliver messages to endpoints.

### Hello World Using A Broker

Our first example creates an endpoint that sends messages to a queue to which it is subscribed. So it both sends and receives its message in one pass.

To start it, simply run:

```
$ ruby ../examples/ruby/reactor/helloworld.rb --address=0.0.0.0:8888 --queue=examples
Hello world!
```

As you can see, the classic message was output by the example. Now let's take a look at what's going on under the covers.

#### Events When Talking To A Broker

The following events occur while **helloworld.rb** runs:

 * **on_start** - Fired when the application is started.
 * **on_sendable** - Fired when a message can be sent.
 * **on_message** - Fired when a message is received.

### Hello World Without A Broker required

The next example we'll look at will send the classic "Hello world" message to itself directly. This example shows some very fundamental elements of the reactor APIs that you should understand.

To launch the example:

```
 $ ruby helloworld_direct.rb --address=0.0.0.0:8888/examples
 Hello world!
```

Not very different from the example that uses the broker, which is what we'd expect from the outside. But let's take a look inside of the example and see how it's different at that level

The direct version takes on the responsibility for listening to incoming connections as well as making an outgoing connection. So we see the following additional events occurring:

 * **on_accepted** - Fired when a message is received.
 * **on_connection_closed** - Fired when an endpoint closes its connection.

## More Complex Reactor Examples

Now that we've covered the basics with the archetypical hello world app, let's look at some more interesting examples.

There are four example applications that demonstrate how to send and receive messages both directly and through an intermediary, such as a broker:

 * **simple_send.rb** - sends messages to a receiver at a specific address and receives responses via an intermediary,
 * **simple_recv.rb** - receives messages from via an intermediary,
 * **direct_send.rb** - sends messages directly to a receiver and listens for responses itself, and
 * **direct_recv.rb** - receives messages directly.

 Simple send and direct send may, at first, seem to be so similar that you wonder why they're not just the same applciation. And I know for me I was wonder as I wrote the list above why there were two examples. The reason is that **simple_send.rb** uses the intermediary transfer responses to the messages it sends, while **direct_send.rb** uses an *Acceptor* to listen for an process responses.

 You can use the examples in the follow ways:

 ```
 simple_send.rb -> broker <- simple_recv.rb
 simple_send.rb -> direct_recv.rb
 direct_send.rb -> simple_recv.rb
 ```

In this set of examples we see the following event occurring, in addition to what we've seen before:

 * **on_disconnected** - Fired when the transport is closed.

## Now About That Broker example

The **broker.rb** example application is a nice demonstration of doing something more interesting in Ruby with Proton.

The way the broker works is to listen to incoming connections, examine the components of the address for that connection, attach that connection to an exchange managing that address and then it sends any messages destined for that address to them.

The components of the broker example include:
 * **Broker** - A class that extends the MessagingHandler class. It accepts incoming connections, manages subscribing them to exchanges, and transfers messages between them.
 * **Exchange** - A class that represents a message queue, tracking what endpoints are subscribed to it.

The Broker manages a map connecting a queue address to the instance of Exchange that holds references to the endpoints of interest.

The broker application demonstrates a new set of reactor events:

 * **on_link_opening** - Fired when a remote link is opened but the local end is not yet open. From this event the broker grabs the address and subscribes the link to an exchange for that address.
 * **on_link_closing** - Fired when a remote link is closed but the local end is still open. From this event the broker grabs the address and unsubscribes the link from that exchange.
 * **on_connection_closing** - Fired when a remote connection is closed but the local end is still open.
 * **on_disconnected** - Fired when the protocol transport has closed. The broker removes all links for the disconnected connection, avoiding workign with endpoints that are now gone.
