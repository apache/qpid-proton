y# *EXPERIMENTAL* Go binding for proton

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
   - Full access to AMQP concepts like connections, sessions and links.

We will follow conventions of the C and python API where possible to help
cross-language developers but idiomatic Go is the overriding consideration.

## Status

The current code is a foundation, not an implementation of the target API.

There are two Go modules so far. See the documentation using

    godoc apache.org/proton
    godoc apache.org/proton/event

The event module contains a straightforward mapping of the proton event API and
the simplified MessagingHandler python API.

The proton module contains the mapping between AMQP types and messages and Go
types.

## The event driven API

The event module contains

- Go Proton events (AMQP events only, no reactor events yet)
- Go MessagingHandler events (equivalent to python MessagingHandler.)
- Pump to feed data between a Go net.Conn connection and a proton event loop.

The Pump uses 3 goroutines per connection, one to read, one to write and one to
run the proton event loop. Proton's thread-unsafe data is never used outside the
event loop goroutine.

This API provides direct access to proton events, equivalent to C or python
event API. It does not yet support reactor events or allow multiple connections
to be handled in a single event loop goroutine, these are temporary limitations.

There is one example: examples/go/broker.go. It is a port of
examples/python/broker.py and can be used with the python `simple_send.py` and
`simple_recv.py` clients.

The broker example works for simple tests but is concurrency-unsafe. It uses a
single `broker`, implementing MessagingHandler, with multiple pumps. The proton
event loops are safe in their separate goroutines but the `broker` state (queues
etc.) is not. We can fix this by funneling multiple connections into a single
event loop as mentioned above.

However this API is not the end of the story. It will be the foundation to build
a more Go-like API that allows *any* goroutine to send or receive messages
without having to know anything about event loops or pumps.

## The Go API

The goal: A procedural API that allows any user goroutine to send and receive
AMQP messages and other information (acknowledgments, flow control instructions
etc.) using channels. There will be no user-visible locks and no need to run
user code in special goroutines, e.g. as handlers in a proton event loop.

There is a (trivial, speculative, incomplete) example in examples/go/example.go
of what part of it might look like. It shows receiving messages concurrently
from two different connections in a single goroutine (it omits sessions). 

There is a tempting analogy between Go channels and AMQP links, but Go channels
are much simpler beasts than AMQP links. It is likely a link will be implemented
by a Go type that uses more than one channel. E.g. there will probably be
separate channels for messages and acknowledgments, perhaps also for flow
control status.

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

