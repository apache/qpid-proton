# Go examples for proton

There are 3 Go packages for proton:

- qpid.apache.org/electron: Concurrent, procedural API for messaging clients and servers.
- qpid.apache.org/proton: Direct access to the event-driven, concurrent-unsafe proton library.
- qpid.apache.org/amqp: Convert AMQP messages and data to and from Go data types.

`proton` and `electron` are alternative APIs for sending messages. `proton` is a
direct wrapping of the concurrent-unsafe, event-driven C proton API. `electron`
is a procedural, concurrent-safe interface that may be more convenient and
familiar for Go programmers. The examples `proton/broker.go` and
`electron/broker.go` give an illustration of how the APIs differ.

## Example programs

electron
- [receive.go](electron/receive.go) receive from many connections concurrently.
- [send.go](electron/send.go) send to many connections concurrently.
- [broker.go](electron/broker.go) a simple broker using the electron API

proton
- [broker.go](proton/broker.go) a simple broker using the proton API

## Using the Go packages

Use `go get qpid.apache.org/electron` or check out the proton repository and set
your GOPATH environment variable to include
`/<path-to-proton>/proton-c/bindings/go`

The proton Go packages include C code so the cgo compiler needs to be able to
find the proton library and include files.  There are a couple of ways to do
this:

1. Build proton in directory `$BUILD`. Source the script `$BUILD/config.sh` to set your environment.

2. Install proton to a standard prefix such as `/usr` or `/usr/local`. No need for further settings.

3. Install proton to a non-standard prefix `$PREFIX`. Set the following:

        export LIBRARY_PATH=$PREFIX/lib:$LIBRARY_PATH
        export C_INCLUDE_PATH=$PREFIX/include:$C_INCLUDE_PATH
        export LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH

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

First start the broker (the optional `-debug` flag will print extra information about
what the broker is doing)

    go run broker.go -debug

Send messages concurrently to queues "foo" and "bar", 10 messages to each queue:

    go run send.go -count 10 localhost:/foo localhost:/bar

Receive messages concurrently from "foo" and "bar". Note -count 20 for 10 messages each on 2 queues:

    go run receive.go -count 20 localhost:/foo localhost:/bar

The broker and clients use the standard AMQP port (5672) on the local host by
default, to use a different address use the `-addr host:port` flag.

If you have the full proton repository checked out you can try try using the
python broker with Go clients:

    python ../python/broker.py

Or use the Go broker and the python clients:

    python ../python/simple_send.py
    python ../python/simple_recv.py


## A tale of two brokers.

The `proton` and `electron` packages provide two alternate APIs for AMQP applications.
See [the proton Go README](https://github.com/apache/qpid-proton/blob/master/proton-c/bindings/go/src/qpid.apache.org/README.md) for a discussion
of why there are two APIs.

The examples `proton/broker.go` and `electron/broker.go` both implement the same
simple broker-like functionality using each of the two APIs. They both handle
multiple connections concurrently and store messages on bounded queues
implemented by Go channels.

However the `electron/broker` is less than half as long as the `proton/broker`
illustrating why it is better suited for most Go applications.

`proton/broker` must explicitly handle proton events, which are processed in a
single goroutine per connection since proton is not concurrent safe. Each
connection uses channels to exchange messages between the event-handling
goroutine and the shared queues that are accessible to all connections. Sending
messages is particularly tricky since we must monitor the queue for available
messages and the sending link for available credit in order to send messages.


`electron/broker` takes advantage of the `electron` package, which hides all the
event handling and passing of messages between goroutines beind behind
straightforward interfaces for sending and receiving messages. The electron
broker can implement links as simple goroutines that loop popping messages from
a queue and sending them or receiving messages and pushing them to a queue.


