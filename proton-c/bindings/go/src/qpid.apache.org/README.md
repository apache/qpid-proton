# Qpid Go Libraries for AMQP

These packages provide [Go](http://golang.org) support for sending and receiving AMQP
messages in client or server applications.

Package documentation is available at: <http://godoc.org/qpid.apache.org/>

See the [examples](https://github.com/apache/qpid-proton/blob/master/examples/go/README.md)
for working examples and practical instructions on how to get started.

Feedback is encouraged at:

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

## Status

There are 3 go packages for proton:

`qpid.apache.org/electron`:  procedural, concurrent-safe Go library for AMQP messaging.
A simple procedural API that can easily be used with goroutines and channels to construct
concurrent AMQP clients and servers.

`qpid.apache.org/proton`: event-driven, concurrent-unsafe Go library for AMQP messaging.
A simple port into Go of the Proton C library. Its event-driven, single-threaded nature
may be off-putting for Go programmers, hence the electron API.

`qpid.apache.org/amqp`: converts AMQP messages and data types to and from Go data types.
Used by both the proton and electron packages to represent AMQP types.

See the
[examples](https://github.com/apache/qpid-proton/blob/master/examples/go/README.md)
for an illustration of the APIs, in particular compare `proton/broker.go` and
`electron/broker.go` which illustrate the different API approaches to the same
task (a simple broker.)


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

The `electron` API hides the event-driven details behind a simple blocking API
that can be safely called from arbitrary goroutines. Under the covers data is
passed through channels to dedicated goroutines running separate `proton` event
loops for each connection.

## New to Go?

If you are new to Go then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and library docs at <http://golang.org> as you need them.
