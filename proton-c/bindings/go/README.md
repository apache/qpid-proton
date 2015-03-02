# *EXPERIMENTAL* Go binding for proton

This is the beginning of a [Go](http://golang.org) binding for proton.

This work is in very early *experimental* stages, *everything* might change in
future.  Comments and contributions are strongly encouraged, this experiment is
public so early feedback can guide development.

- Email <proton@qpid.apache.org>
- Create issues <https://issues.apache.org/jira/browse/PROTON>, attach patches to an issue.

## Goals

The API will be inspired by the reactive, event-driven python API. Key features:

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
cross-language developers but idiomatic Go is the overriding consideration.

## Status

So just a simple Go `Url` type using `pn_url_t`.  This establishes the basics of
using cgo to call into proton code.

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

## Implementation status

Working on API to marshal/unmarshal AMQP data into Go types.

The API will follow the style of the standard libraries encoding/json and encoding/xml.

To be done:

Easy unmarshaling into native Go types:

- String-like AMQP types (symbol, binary, string) into Go string or []byte
- Numeric AMQP types into any Go numeric (numeric conversion)
- Any AMQP type into GO reflect.Value choosing the closest native Go type
- AMQP map into go struct if keys match struct field names and values match field types
- AMQP maps into map[K]T if all AMQP keys/values can convert to K and T (reflect.Value allowed)
- AMQP list into []T if all list elements can convert to T (reflect.Value allowed)

Easy marshaling of native Go types:

- Go struct: amqp map with field names as string keys (use tags to customize?)
- Go string to AMQP string, Go []byte to AMQP binary, Go numerics to closest AMQP numeric
- Go []T to AMQP list
- Go map to AMQP map

Customization:

- Standard encoding libraries define Marshaler and Unmarshaler interfaces.
- User implements to customize behavior of a user type.
- Does this require exposing the proton codec?

Exact (strict) (un)marshaling:

- Define special Go AMQP types that exactly reflect AMQP types & encodings.
- Unmarshal to special types only if exact match for wire
- Marshal special types exactly
- Define AMQPValue which can unmarshal from any AMQP type using strict unmarshaling types.

