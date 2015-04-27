# *EXPERIMENTAL* Go binding for proton

This is the beginning of a [Go](http://golang.org) binding for proton.

This work is in early *experimental* stages, *everything* may change in future.
Comments and contributions are strongly encouraged, this experiment is public so
early feedback can guide development.

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

## Goals

The API should

- be idiomatic, unsurprising, and easy to use for Go developers.
- support client and server development.
- make simple tasks simple.
- provide deep access to AMQP protocol when that is required.

There are two types of developer we want to support

1. Go developers using AMQP as a message transport:
   - Straightforward conversions between Go built-in types and AMQP types.
   - Easy message exchange via Go channels to support use in goroutines.

2. AMQP-aware developers using Go as an implementation language:
   - Go types to exactly represent all AMQP types and encoding details.
   - Full access to detailed AMQP concepts: sessions, links, deliveries etc.

## Status

Package proton encodes and decodes AMQP messages and data as Go types.

Sub-packages 'event' and 'messaging' provide two alternative ways to write
AMQP clients and servers. 'messaging' is easier for general purpose use. 'event'
gives complete low-level control of the underlying proton C engine.

The event package is fairly complete, with the exception of the proton
reactor. It's unclear if the reactor is important for go.

The messaging package is just starting. The examples work but anything else might not.

### Examples

messaging API:

- [receive.go](../../../examples/go/receive.go) receive from many connections concurrently
- [send.go](../../../examples/go/send.go) send to many connections concurrently

event API:
- [broker.go](../../../examples/go/event/broker.go) simple mini-broker

The examples work with each other and with the python `broker.py`,
`simple_send.py` and `simple_receive.py`.

## The event driven API

See the package documentation for details.

## The Go API

The goal: A procedural API that allows any user goroutine to send and receive
AMQP messages and other information (acknowledgments, flow control instructions
etc.) using channels. There will be no user-visible locks and no need to run
user code in special goroutines, e.g. as handlers in a proton event loop.

See the package documentation for emerging details.

Currently using a channel to receive messages, a function to send them (channels
internally) and a channel as a "future" for acknowledgements to senders. This
may change.

## Design Questions


1. Error reporting and handling, esp. async. errors:

What are common patterns for handling errors across channels?  I.e. the thing at
one end of the channel blows up, how do you tell the other end?

readers: you can close the channel, but there's no error info. You could pass a
struct { data, error } or use a second channel. Pros & cons?

writers: you can't close without a panic so you need a second channel.  Is this
a normal pattern:

    select {
        data -> sendChan: sentit()
        err := <- errChan: oops(err)
    }

2. Use of channels:

I recently saw an interesting Go tip: "Make your API synchronous because in Go
it is simple to make a sync call async by putting it in a goroutine."

What are the tradeoffs of exposing channels directly in the API vs. hiding them
behind methods? Exposing lets users select directly, less overhead than starting
a goroutine, creating MORE channels and selecting those. Hiding lets us wrap
patterns like the 'select {data, err}' pattern above, which is easier and less
error prone than asking users to do it themselves.

The standard net.Conn uses blocking methods, not channels. I did as the tip says
and wrapped them in goroutines and channels. The library does expose *read*
channels e.g. time.After. Are there any *write* channels in the standard
library? I note that time.After has no errors, and suspect that may be a key
factor in the descison.

3. The "future" pattern for acknowledgements: super easy in Go but depends on 1. and 2. above.

## Why a separate API for Go?

Go is a concurrent language and encourages applications to be divided into
concurrent *goroutines*. It provides traditional locking but it encourages the
use *channels* to communicate between goroutines without explicit locks:

  "Share memory by communicating, don't communicate by sharing memory"

The idea is that a given value is only operated on by one goroutine at a time,
but values can easily be passed from one goroutine to another. This removes much
of the need for locking.

Go literature distinguishes between:

- *concurrency*: "keeping track of things that could be done in parallel"
- *parallelism*: "actually doing things in parallel"

The application expresses concurrency by starting goroutines for potentially
concurrent tasks. The Go run-times schedule the activity of goroutines onto a
small number (possibly one) of actual parallel executions.

Even with *no* parallelism, concurrency lets the Go run-times *order* work with
respect to events like file descriptors being readable/writable, channels having
data, timers firing etc. Go automatically takes care of switching out goroutines
that block or sleep so it is normal to write code in terms of blocking calls.

Event-driven API (like poll, epoll, select or the proton event API) also
channel unpredictably ordered events to actions in one or a small pool of
execution threads. However this requires a different style of programming:
"event-driven" or "reactive" programming. Go developers call it "inside-out"
programming. In an event-driven architecture blocking is a big problem as it
consumes a scarce thread of execution, so actions that take time to complete
have to be re-structured in terms of future event delivery.

The promise of Go is that you can express your application in concurrent,
procedural terms with simple blocking calls and the Go run-times will turn it
inside-out for you. Write as many goroutines as you want, and let Go interleave
and schedule them efficiently.

For example: the Go equivalent of listening for connections is a goroutine with
a simple endless loop that calls a blocking Listen() function and starts a
goroutine for each new connection. Each connection has its own goroutine that
deals with just that connection till it closes.

The benefit is that the variables and logic live closer together. Once you're in
a goroutine, you have everything you need in local variables, and they are
preserved across blocking calls. There's no need to store details in context
objects that you have to look up when handling a later event to figure out how
to continue where you left off.

So a Go-like proton API does not force the users code to run in an event-loop
goroutine. Instead user goroutines communicate with the event loop(s) via
channels.  There's no need to funnel connections into one event loop, in fact it
makes no sense.  Connections can be processed concurrently so they should be
processed in separate goroutines and left to Go to schedule. User goroutines can
have simple loops that block channels till messages are available, the user can
start as many or as few such goroutines as they wish to implement concurrency as
simple or as complex as they wish. For example blocking request-response
vs. asynchronous flows of messages and acknowledgments.


## Layout

This directory is a [Go work-space](http://golang.org/doc/code.html), it is not
yet connected to the rest of the proton build.

To experiment, install proton in a standard place or set these environment
variables: `PATH`, `C_INCLUDE_PATH`, `LIBRARY_PATH` and `LD_LIBRARY_PATH`.

Add this directory to `GOPATH` for the Go tools.

To see the docs as text:

    godoc apache.org/proton

To see them in your browser run this in the background and open
http://localhost:6060 in your browser:

    godoc -http=:6060 -index=true&

Click "Packages" and "proton" to see the proton docs. It takes a minute or two
to generate the index so search may not work immediately.

To run the unit tests:

    go test -a apache.org/proton

## New to Go?

If you are new to Go then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and library docs at <http://golang.org> as you need them.

