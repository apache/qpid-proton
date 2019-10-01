# Go examples

## Electron examples

[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) is a
simple API for writing concurrent AMQP clients and servers.

- [receive.go](electron/receive.go) receive from many connections concurrently.
- [send.go](electron/send.go) send to many connections concurrently.
- [broker.go](electron/broker.go) a simple broker using the electron API

## Proton examples

[proton](http://godoc.org/github.com/apache/qpid-proton/go/pkg/proton) is an
event-driven, concurrent-unsafe Go wrapper for the proton-C library. The
[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) package provides a more
Go-friendly concurrent API built on top of proton.

- [broker.go](proton/broker.go) a simple broker using the proton API

See [A Tale of Two Brokers](#a-tale-of-two-brokers) for a comparison of the two APIs.

## Using the Go packages

If you have the proton-C library and headers installed you can get the latest go
packages with

    go get github.com/apache/qpid-proton/go/pkg/electron

If Proton-C is installed in a non-standard place (other than /usr or /usr/local)
you should set these environment variables before `go get`:

    export CGO_LDFLAGS="-L/<my-proton>/lib[64]"
    export CGO_CFLAGS="-I/<my-proton>/include"
    go get github.com/apache/qpid-proton

If you have a proton build you don't need to `go get`, you can set your GOPATH
to use the binding from the checkout with:

    source <path-to-proton>/config.sh

Once you are set up, the go tools will work as normal. You can see documentation
in your web browser at `localhost:6060` by running:

    godoc -http=:6060

## Running the examples

You can run the examples directly from source like this:

    go run <program>.go

This is a little slow (a couple of seconds) as it compiles the program and runs it in one step.
You can compile the program first and then run the executable to avoid the delay:

    go build <program>.go
    ./<program>

All the examples take a `-h` flag to show usage information, and the comments in
the example source have more details.

First start the broker on the default AMQP port 5672 (the optional `-debug` flag
will print extra information about what the broker is doing)

    go run broker.go -addr=:5672 -debug   # Use a different port if 5672 is not available.

Send messages concurrently to queues "foo" and "bar", 10 messages to each queue:

    go run send.go -count 10 amqp://localhost:5672/foo amqp://localhost:5672/bar

Receive messages concurrently from "foo" and "bar". Note -count 20 for 10 messages each on 2 queues:

    go run receive.go -count 20 amqp://localhost:5672/foo amqp://localhost:5672/bar

The broker and clients use the standard AMQP port (5672) on the local host by
default, to use a different address use the `-addr host:port` flag.

If you have other Proton examples available you can try communicating between
programs in in different languages. For example use the python broker with Go
clients:

    python ../python/broker.py
    go run send.go -count 10 localhost:/foo localhost:/bar

Or use the Go broker and the python clients:

    go run broker.go -debug
    python ../python/simple_send.py
    python ../python/simple_recv.py


## A tale of two brokers.

The [proton](http://godoc.org/github.com/apache/qpid-proton/go/pkg/proton) and
[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) packages provide two
different APIs for building AMQP applications. For most applications,
[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) is easier to use.

The examples [proton/broker.go](proton/broker.go) and
[electron/broker.go](electron/broker.go) implement the same simple broker
functionality using each of the two APIs. They both handle multiple connections
concurrently and store messages on bounded queues implemented by Go channels.

However the [electron/broker.go](electron/broker.go) is less than half as long as the
[proton/broker.go](proton/broker.go) illustrating why it is better suited for most Go
applications.

[proton/broker.go](proton/broker.go) implements an event-driven loop per connection that reacts
to events like 'incoming link', 'incoming message' and 'sender has credit'.  It
uses channels to exchange data between the event-loop goroutine for each
connection and shared queues that are accessible to all connections. Sending
messages is particularly tricky, the broker must monitor the queue for available
messages and the sender link for available credit.


[electron/broker.go](electron/broker.go) does not need any "upside-down"
event-driven code, it is implemented as straightforward loops. The broker is a
loop listening for connections. Each connection is a loop accepting for incoming
sender or receiver links. Each receiving link is a loop that receives a message
and pushes it to a queue.  Each sending link is a loop that pops a message from
a queue and sends it.

Queue bounds and credit manage themselves: popping from a queue blocks till
there is a message, sending blocks until there is credit, receiving blocks till
something is received and pushing onto a queue blocks until there is
space. There's no need for code that monitors the state of multiple queues and
links. Each loop has one simple job to do, and the Go run-time schedules them
efficiently.

