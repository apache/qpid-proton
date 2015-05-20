# Go examples for proton

There are 3 go packages for proton:

- qpid.apache.org/proton/go/amqp: converts AMQP messages and data types to and from Go data types.
- qpid.apache.org/proton/go/messaging: easy-to-use, concurrent API for messaging clients and servers.
- qpid.apache.org/proton/go/event: full low-level access to the proton engine.

Most applications should use the `messaging` package. The `event` package is for
applications that need low-level access to the proton engine.

## Example programs

- [receive.go](receive.go) receive from many connections concurrently using messaging package.
- [send.go](send.go) send to many connections concurrently using messaging package.
- [event_broker.go](event_broker.go) simple mini-broker using event package.

## Using the Go packages

Set your GOPATH environment variable to include `/<path-to-proton>/proton-c/bindings/go`

The proton Go packages include C code so the cgo compiler needs to be able to
find the proton library and include files.  There are a couple of ways to do this:

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

First start the broker:

    go run event_broker.go

Send messages concurrently to queues "foo" and "bar", 10 messages to each queue:

    go run send.go -count 10 localhost:/foo localhost:/bar

Receive messages concurrently from "foo" and "bar". Note -count 20 for 10 messages each on 2 queues:

    go run receive.go -count 20 localhost:/foo localhost:/bar

The broker and clients use the amqp port on the local host by default, to use a
different address use the `-addr host:port` flag.

You can mix it up by running the Go clients with the python broker:

    python ../python/broker.py

Or use the Go broker and the python clients:

    python ../python/simple_send.py
    python ../python/simple_recv.py`.
