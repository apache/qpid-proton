# Go binding for proton

This is the (very early) beginning of a go binding for proton.

## Goals

The API will be inspired by the reactive, event-driven python API. Key features
to preserve:

- support client and server development.
- incremental composition of functionality via handlers.
- default handlers to make simple tasks simple.
- deep access to AMQP protocol events when that is required.

The API will be idiomatic, unsurprising, and easy to use for Go developers.

There are two types of developer we want to support

1. For Go developers using AMQP as a message transport:
   - Straightforward conversions between Go built-in types and AMQP types.
   - Easy message exchange via Go channels to support use in goroutines.

2. For AMQP-aware developers using Go as an implementation language:
   - Go types to exactly represent all AMQP types and encoding details.
   - Full access to AMQP concepts like connections, sessions and links via handler interfaces.

We will follow conventions of the C and python API where possible to help
multi-language developers but idiomatic Go is the overriding consideration.

## Layout

The plan is to adopt standard Go tools and practices, see the <http://golang.org>
for what that means.

This directory is layered out as a [Go work-space](http://golang.org/doc/code.html)
Only the `src/` directory is committed. Tools like `go build` will create output
directories such as `pkg` and `bin`, they must not be committed.

Set `GOPATH=<this directory>[:<other directories>]` in your environment for the go tools.

We will eventually have a cmake integration to drive the go tool-chain so that:

- Users who don't know/care about Go can build, test & install everything with a single tool (cmake)
- Go developers can contribute using familiar Go tools.

## New to Go?

If you are new to Go (like me) then these are a good place to start:

- [A Tour of Go](http://tour.golang.org)
- [Effective Go](http://golang.org/doc/effective_go.html)

Then look at the tools and library docs at <http://golang.org> as you need them.

Here are some things you can try. First set your environment:

    export GOPATH=<path to this directory>

To see the docs as text:

    godoc apache.org/proton

To see them in your browser run this in the background and open
http://localhost:6060 in your browser:

    godoc -http=:6060 -index=true&

This gives you the full standard Go library documentation plus the proton
docs. The index takes a few minutes to generate so search may not work
immediately, but you can click "Packages" and "proton" to go straight to the
proton docs.

To run the unit tests:

    go test -a apache.org/proton

## Design Notes

### C wrapping philosophy

We use `cgo` to call proton C functions directly. `cgo` is simpler and more
direct than Swig and integrated into the Go tools.

Calling C directly from Go is so easy that we will avoid low-level 1-1 wrappers
for proton objects and focus on easy to use Go types that live within Go's
automatic memory management. 

Programmers that need lower-level access than we provide can go direct to C, but
of course we will aim to make that unnecessary in all but the most unusual cases.

###  Other considerations

Go's use of channels for synchronization present interesting opportunities. Go
also supports traditional locking, so we could adopt locking strategies similar
to our other bindings, but we should investigate the Go-like alternatives. There
are analogies between Go channels and AMQP links that we will probably exploit.

## State of the implementation

So far we have a wrapper for `pn_url_t` with unit tests and docs. This just
gives us an initial work-space and exercise for the tools and establishes the
basics of using cgo to call into proton code.


