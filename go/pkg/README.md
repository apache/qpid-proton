# Qpid Go packages for AMQP

These packages provide [Go](https://golang.org) support for sending and receiving
AMQP messages in client or server applications. Reference documentation is
available at: <https://godoc.org/github.com/apache/qpid-proton>

They require the
[proton-C library and header files](http://qpid.apache.org/proton) to be
installed.  On many platforms it is available pre-packaged, for example on
Fedora

    dnf install qpid-proton-c-devel

If you built proton from source, you can set environment variables to find the
built libraries and headers as follows:

    source <build-directory>/config.sh

If you have installed the library and headers in non-standard directories, then
add them to the following environment variables:

    LD_LIBRARY_PATH  # directory containing the library
    LIBRARY_PATH     # directory containing the library
    C_INCLUDE_PATH   # directory containing the proton/ subdirectory with header files

There are 3 packages:

[amqp](http://godoc.org/github.com/apache/qpid-proton/go/pkg/amqp) provides functions
to convert AMQP messages and data types to and from Go data types.  Used by both
the proton and electron packages to manage AMQP data.

[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) is a
simple, concurrent-safe API for sending and receiving messages. It can be used
with goroutines and channels to build concurrent AMQP clients and servers.

[proton](http://godoc.org/github.com/apache/qpid-proton/go/pkg/proton) is an
event-driven, concurrent-unsafe package that closely follows the proton C
API. Most Go programmers will find the
[electron](http://godoc.org/github.com/apache/qpid-proton/go/pkg/electron) package easier to use.

See the [examples](https://github.com/apache/qpid-proton/blob/master/go/examples/README.md)
to help you get started.

Feedback is encouraged at:

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

### Why two APIs?

The `proton` API is a direct mapping of the proton C library into Go. It is
usable but not very natural for a Go programmer because it takes an
*event-driven* approach and has no built-in support for concurrent
use. `electron` uses `proton` internally but provides a more Go-like API that is
safe to use from multiple concurrent goroutines.

Go encourages programs to be structured as concurrent *goroutines* that
communicate via *channels*. Go literature distinguishes between:

- *concurrency*: "keeping track of things that could be done in parallel"
- *parallelism*: "actually doing things in parallel on multiple CPUs or cores"

A Go program expresses concurrency by starting goroutines for potentially
concurrent tasks. The Go runtime schedules the activity of goroutines onto a
small number (possibly one) of actual parallel executions.

Even with no hardware parallelism, goroutine concurrency lets the Go runtime
order unpredictable events like file descriptors being readable/writable,
channels having data, timers firing etc. Go automatically takes care of
switching out goroutines that block or sleep so it is normal to write code in
terms of blocking calls.

By contrast, event-driven programming is based on polling mechanisms like
`select`, `poll` or `epoll`. These also dispatch unpredictably ordered events to
a single thread or a small thread pool. However this requires a different style
of programming: "event-driven" or "reactive" programming. Go developers call it
"inside-out" programming.  In an event-driven program blocking is a big problem
as it consumes a scarce thread of execution, so actions that take time to
complete have to be re-structured in terms of multiple events.

The promise of Go is that you can express your program in concurrent, sequential
terms and the Go runtime will turn it inside-out for you. You can start
goroutines for all concurrent activities. They can loop forever or block for as
long as they need waiting for timers, IO or any unpredictable event. Go will
interleave and schedule them efficiently onto the available parallel hardware.

For example: in the `electron` API, you can send a message and wait for it to be
acknowledged in a single function. All the information about the message, why
you sent it, and what to do when it is acknowledged can be held in local
variables, all the code is in a simple sequence. Other goroutines in your
program can be sending and receiving messages concurrently, they are not
blocked.

In the `proton` API, an event handler that sends a message must return
*immediately*, it cannot block the event loop to wait for
acknowledgement. Acknowledgement is a separate event, so the code for handling
it is in a different event handler. Context information about the message has to
be stored in some non-local variable that both functions can find. This makes
the code harder to follow.

The `proton` API is important because it is the foundation for the `electron`
API, and may be useful for programs that need to be close to the original C
library for some reason. However the `electron` API hides the event-driven
details behind simple, sequential, concurrent-safe methods that can be called
from arbitrary goroutines. Under the covers, data is passed through channels to
dedicated `proton` goroutines so user goroutines can work concurrently with the
proton event-loop.

## New to Go?

If you are new to Go then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and docs at <http://golang.org> as you need them.
