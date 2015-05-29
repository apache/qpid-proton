# *EXPERIMENTAL* Go binding for proton

This is an *experimental* [Go](http://golang.org) binding for proton.
The API is subject to change but there is enough to get a good idea of where it is headed.

Feedback is strongly encouraged:

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

The package documentation is available at: <http://godoc.org/qpid.apache.org/proton/go>

See the [examples](../../../examples/go/README.md) for working examples and
practical instructions on how to get started.

The rest of this page discusses the high-level goals and design issues.

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

There are 3 go packages for proton:

- qpid.apache.org/proton/go/amqp: converts AMQP messages and data types to and from Go data types.
- qpid.apache.org/proton/go/messaging: easy-to-use, concurrent API for messaging clients and servers.
- qpid.apache.org/proton/go/event: full low-level access to the proton engine.

Most applications should use the `messaging` package. The `event` package is for
applications that need low-level access to the proton engine.

The `event` package is fairly complete, with the exception of the proton
reactor. It's unclear if the reactor is important for go.

The `messaging` package can run the examples but probably not much else. There
is work to do on error handling and the API may change.

There are working [examples](../../../examples/go/README.md) of a broker using `event` and
a sender and receiver using `messaging`.

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

## New to Go?

If you are new to Go then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and library docs at <http://golang.org> as you need them.
