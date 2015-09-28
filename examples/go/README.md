# Go examples for proton

There are 3 Go packages for proton:

- qpid.apache.org/proton/concurrent: Easy-to-use, concurrent API for concurrent clients and servers.
- qpid.apache.org/proton/amqp: Convert AMQP messages and data to and from Go data types.
- qpid.apache.org/proton: Direct access to the event-driven, concurrent-unsafe proton library.

Most applications should use the `concurrent` package. The `proton` package is
for applications that need low-level access to the proton library.

## Example programs

- [receive.go](receive.go) receive from many connections concurrently.
- [send.go](send.go) send to many connections concurrently.

## Using the Go packages

Use `go get qpid.apache.org/proton/concurrent` or check out the proton
repository and set your GOPATH environment variable to include
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
