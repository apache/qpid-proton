# Go examples for proton

The Go support for proton consists of 3 packages:

- proton: converts AMQP messages and data types to and from Go data types.
- proton/messaging: easy-to-use, concurrent API for messaging clients and servers.
- proton/event: full low-level access to the proton engine.

Most applications should use the proton/messaging API. proton/event is for
applications that need low-level access to the proton engine. proton/messaging
itself is implemented using proton/event.

## proton/messaging examples

- [receive.go](receive.go) receive from many connections concurrently.
- [send.go](send.go) send to many connections concurrently.

## proton/event examples

- [broker.go](event/broker.go) simple mini-broker, queues are created automatically.

## Running the examples

Proton needs to be installed in a standard place such as `/usr` or `/usr/local`.
(in future the examples will be able to use the local proton build)

Set your environment:

    export GOPATH=<path-to-proton-checkout>/proton-c/bindings/go

You can run the examples directly from source with

    go run <program>.go

This is a little slow (a couple of seconds) as it compiles the program and runs it in one step.
You can compile the program first and then run the executable to avoid the delay:

    go build <program>.go
    ./<program>

All the examples take a `-h` flag to show usage information, see comments in the example
source for more details.

## Example of running the examples.

First start the broker:

    go run event/broker.go

Send messages concurrently to queues "foo" and "bar", 10 messages to each queue:

    go run go/send.go -count 10 localhost:/foo localhost:/bar

Receive messages concurrently from "foo" and "bar". Note -count 20 for 10 messages each on 2 queues:

    go run go/receive.go -count 20 localhost:/foo localhost:/bar

The broker and clients use the amqp port on the local host by default, to use a
different address use the `-addr host:port` flag.

You can mix it up by running the Go clients with the python broker:

    python ../python/broker.py

Or use the Go broker and the python clients:

    python ../python/simple_send.py
    python ../python/simple_recv.py`.

