## Simple Messenger Examples

The Messenger APIs, contained in the Qpid::Proton::Messenger class, represent the simplest means for sending and receive messages. An application need only create an instance of Messenger to have an endpoint for sending and receiving messages.

Example applications, currently located in the messenger subdirectory, include:

* **send.rb** - Sends one or more messages to a specific address.
* **recv.rb** - Listens for messages sent to one or more addresses.

### Sending A Simple Message Directly (Without A Broker)

The goal of this example is to demonstrate how to send and receive messages directly between applications. To do that we've broken out the examples into two applciations: **send.rb** and **recv.rb**.

First we need to start the receiver who will listen for an incoming connection:

```
 $ ruby recv.rb ~0.0.0.0:8888
```

**NOTE:** Be sure to include the **tilda (~)** at the beginning of each address to be used. This tells the Messenger that it's going to be listening for connections on that port. And be sure to pick a free port for each of the addresses used.

Now you can send messages with:

```
 $ ruby send.rb -a 0.0.0.0:8888 "Hello world"
```

**NOTE:** Here you *don't* use a **tilda (~)** at the beginning of the address since you're specifying the receiver address rather than the address on which to listen.

On the receiver side you should see something like:

```
Address: 0.0.0.0:8888
Subject: How are you?
Body: This is a test.
Properties: {"sent"=>1432651492, "hostname"=>"mcpierce-laptop"}
Instructions: {"fold"=>"yes", "spindle"=>"no", "mutilate"=>"no"}
Annotations: {"version"=>1.0, "pill"=>"RED"}

```

This tells us the message we received as expected.

### Sending A Simple Message Through A Broker

To send a message via an intermediary, such as the Qpid broker, we need to give both the sender and the receiver a little bit more information, such as the hostname and port for the broker and the name of the queue where messages will be sent and received.

If you're using the Qpid broker, this can be done using the **qpid-config**
tool. Simply start your broker and then create our queue, which we'll call "examples":

```
 $ qpid-config add queue examples
```

As of this point you can begin sending messages *without* starting the receiver. This is the benefit of using a broker, the ability to have something store messages for retrieval.

Now let's send a bunch of messages to the queue using **send.rb**:

```
 $ for which in $(seq 1 10); do ruby send.rb --address amqp://localhost/examples "This is test ${which}."; done
```

This example sends 10 messages to the our queue on the broker, where they will stay until retrieved.

With this scenario we needed to specify the hostname of the broker and also the name of the queue in the address. After the sending completes you can verify the content of the queue using the **qpid-stat** command:

```
$ qpid-stat -q
Queues
  queue                                     dur  autoDel  excl  msg   msgIn  msgOut  bytes  bytesIn  bytesOut  cons  bind
  =========================================================================================================================
  examples                                                        10    10      0    2.65k  2.65k       0         0     1
  fa4b8acb-03b6-4cff-9277-eeb2efe3c88a:0.0       Y        Y        0     0      0       0      0        0         1     2
```

Now to retrieve the messages. Start the **recv.rb** example using:

```
$ ruby recv.rb "amqp://127.0.0.1:5672/examples"
```

Once it connects to the broker, it will subscribe to the queue and begin receiving messages. You should see something like the following:

```
Address: amqp://localhost/examples
Subject: How are you?
Body: This is test 1.
Properties: {"sent"=>1432837902, "hostname"=>"mcpierce-laptop"}
Instructions: {"fold"=>"yes", "spindle"=>"no", "mutilate"=>"no"}
Annotations: {"version"=>1.0, "pill"=>"RED"}
Address: amqp://localhost/examples
Subject: How are you?
Body: This is test 2.
Properties: {"sent"=>1432837903, "hostname"=>"mcpierce-laptop"}
Instructions: {"fold"=>"yes", "spindle"=>"no", "mutilate"=>"no"}
Annotations: {"version"=>1.0, "pill"=>"RED"}
Address: amqp://localhost/examples
Subject: How are you?
Body: This is test 3.
... truncating the remainder of the output ...
```

As with the sending, we specific the protocol to use and the address and port on the host to connect to the broker.

##  Non-Blocking Receive With The Messenger APIs

One of the downsides of Messenger is that, out of the box, it's a blocking set of APIs; i.e., when attempting to receive messages the Ruby VM is halted until the call returns.

Enter **non-blocking mode**!

When working in non-blocking mode you set a flag on your messenger to put it into non-blocking mode.

```
messenger = Qpid::Proton::Messenger.new
messenger.passive = true
```

At this point the application is responsible for monitoring the set of file descriptors associated with those Messengers. It takes a little more code but the benefit is that the application can spawn this in a separate thread and not have the entire VM block during I/O operations. The **nonblocking_recv.rb** example app demonstrates how to perform this properly, how to wrap the file descriptor in a Selectable type, monitor it for when there's data to send or receive and then work with the instance of Messenger.

For more details on how to do this, take a look at the comments in the example application.


### Running The Non-Blocking Receiver

To start the non-blocking receiver, simply start the example application like this:

```
 $ ruby nonblocking_recv.rb ~0.0.0.0:8888
 This is a side thread:
The time is now 11:39:24.
The time is now 11:39:25.
The time is now 11:39:26.
```

As the receiver runs it outputs the current time every second to show that a separate thread is running.

Now you can launch the **client.rb** example to send a message to the receiver and then wait for a reply:

```
 $ ruby client.rb -r "~/#" 0.0.0.0:8888 "This is a test." "Here is my message."
 amqp://c19f3e88-866a-46ae-8284-d229acf8a5cb/#, RE: This is a test.
```

As you see, in the command line we specify "~/#" as the address to which any reply should be sent. It's outside of the scope of this example, but the tilda (~) is expanded into a unique address by Proton and used for receiving any responses.

On the receiver side you should see something like this:

```
The time is now 11:42:04.
The time is now 11:42:05.
The time is now 11:42:06.
Address: 0.0.0.0:8888
Subject: This is a test.
Body:
Properties: {}
Instructions: {}
Annotations: {}
=== Sending a reply to amqp://c19f3e88-866a-46ae-8284-d229acf8a5cb/#
The time is now 11:42:07.
The time is now 11:42:08.
```

Notice in the receiver's output how the reply was sent to the address on the sender that was created by our specifying "~/#"
