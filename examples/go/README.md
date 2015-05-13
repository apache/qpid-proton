# Go examples for proton

There are 3 go packages for proton:

- qpid.apache.org/proton/go/amqp: converts AMQP messages and data types to and from Go data types.
- qpid.apache.org/proton/go/messaging: easy-to-use, concurrent API for messaging clients and servers.
- qpid.apache.org/proton/go/event: full low-level access to the proton engine.

Most applications should use the `messaging` package. The `event` package is for
applications that need low-level access to the proton engine.

## messaging examples

- [receive.go](receive.go) receive from many connections concurrently.
- [send.go](send.go) send to many connections concurrently.

## event examples

- [broker.go](event/broker.go) simple mini-broker, queues are created automatically.

## Installing the proton Go packages

You need to install proton in a standard place such as `/usr` or `/usr/local` so go
can find the proton C headers and libraries to build the C parts of the packages.

You should create a go workspace and set GOPATH as described in https://golang.org/doc/code.html

To get the proton packages into your workspace you can clone the proton repository like this:

    git clone https://git.apache.org/qpid-proton.git $GOPATH/src/qpid.apache.org/proton

If you prefer to keep your proton clone elsewhere you can create a symlink to it in your workspace.

You can also use `go get` as follows:

    go get qpid.apache.org/proton/go/messaging

Once installed you can use godoc to look at docmentation on the commane line or start a
godoc web server like this:

	godoc -http=:6060

And look at the docs in your browser.

Right now the layout of the documentation is a bit messed up, showing the long
path for imports, i.e.

    qpid.apache.org/proton/proton-c/bindings/go/amqp

In your code you should use:

    qpid.apache.org/proton/go/amqp


## Running the examples

You can run the examples directly from source like this:

    go run <program>.go

This is a little slow (a couple of seconds) as it compiles the program and runs it in one step.
You can compile the program first and then run the executable to avoid the delay:

    go build <program>.go
    ./<program>

All the examples take a `-h` flag to show usage information, and the comments in
the example source have more details.

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
