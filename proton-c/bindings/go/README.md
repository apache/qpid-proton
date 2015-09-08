# *EXPERIMENTAL* Go binding for proton

This is an *experimental* [Go](http://golang.org) binding for proton.
The API is subject to change but there is enough to get a good idea of where it is headed.

Feedback is strongly encouraged:

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

The package documentation is available at: <http://godoc.org/qpid.apache.org/proton>

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

- qpid.apache.org/proton/amqp: converts AMQP messages and data types to and from Go data types.
- qpid.apache.org/proton/concurrent: easy-to-use, concurrent API for clients and servers.
- qpid.apache.org/proton: full low-level access to the proton engine.

The `amqp` package provides conversions between AMQP and Go data types that are
used by the other two packages.

The `concurrent` package provides a simple procedural API that can be used with
goroutines to construct concurrent AMQP clients and servers.

The `proton` package is a concurrency-unsafe, event-driven API. It is a very
thin wrapper providing almost direct access to the underlying proton C API.

The `concurrent` package will probably be more familiar and convenient to Go
programmers for most use cases. The `proton` package may be more familiar if
you have used proton in other languages.

Note the `concurrent` package itself is implemented in terms of the `proton`
package. It takes care of running concurrency-unsafe `proton` code in dedicated
goroutines and setting up channels to move data between user and proton
goroutines safely. It hides all this complexity behind a simple procedural
interface rather than presenting an event-driven interface.

See the [examples](../../../examples/go/README.md) for a better illustration of the APIs.

### Why two APIs?

Go is a concurrent language and encourages applications to be divided into
concurrent *goroutines*. It provides traditional locking but it encourages the
use *channels* to communicate between goroutines without explicit locks:

  "Share memory by communicating, don't communicate by sharing memory"

The idea is that a given value is only operated on by one goroutine at a time,
but values can easily be passed from one goroutine to another.

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

Event-driven programming (such as poll, epoll, select or the `proton` package)
also channels unpredictably ordered events to actions in one or a small pool of
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

The `proton` API is important because it is close to the original proton-C
reactive API and gives you direct access to the underlying library. However it
is un-Go-like in it's event-driven nature, and it requires care as methods on
values associated with the same underlying proton engine are not
concurrent-safe.

The `concurrent` API hides the event-driven details behind a simple blocking API
that can be safely called from arbitrary goroutines. Under the covers data is
passed through channels to dedicated goroutines running separate `proton` event
loops for each connection.

### Design of the concurrent API

The details are still being worked out (see the code) but some basic principles have been
established.

Code from the `proton` package runs _only_ in a dedicated goroutine (per
connection). This makes it safe to use proton C data structures associated with
that connection.

Code in the `concurrent` package can run in any goroutine, and holds `proton`
package values with proton object pointers.  To use those values, it "injects" a
function into the proton goroutine via a special channel. Injected functions
can use temporary channels to allow the calling code to wait for results. Such
waiting is only for the local event-loop, not across network calls.

The API exposes blocking calls returning normal error values, no exposed
channels or callbacks. The user can write simple blocking code or start their
own goroutine loops and channels as appropriate. Details of our internal channel
use and error handling are hidden, which simplifies the API and gives us more
implementation flexibility.

TODO: lifecycle rules for proton objects.

## New to Go?

If you are new to Go then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and library docs at <http://golang.org> as you need them.
