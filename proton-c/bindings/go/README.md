# Go binding for proton

This is the (very early) beginning of a go binding for proton.

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

The docs are short, readable and informative.  Even the language specification
<http://golang.org/ref/spec> is a readable and useful reference.

My first impression is that Go is a little odd but approachable and has some
very interesting features. I look forward to writing some real code.

Go has simple but effective tools for building, testing, documenting,
formatting, installing etc. So far I have used `go build`, `go run`, `go test`
and `godoc`. They all Just Worked with little or no head-scratching.  For
example, within 5 minutes of discovering `godoc` I had a searchable web site on
my laptop serving all the standard Go docs and the docs for the fledgling
proton package.

"Simple but effective" seems to be a big part of the Go design philosophy.

## Design of the binding

The API will be based on the design of the reactive python API, some key features to preserve are:

- client and server development.
- reactive and synchronous programming styles.
- incremental composition of functionality via handlers.
- default handlers to make simple tasks simple.
- deep access to AMQP protocol events when that is required.

Go is it's own  language, and we have two goals to balance:

- avoid needless deviation from existing APIs to lower the barrier to move between languages.
- present an idiomatic, easy-to-use, unsurprising Go API that takes full advantage of Go's features.

The implementation will use `cgo` to call into the proton C reactor and other
proton C APIs. `cgo` is simpler than Swig, requires only knowledge of Go and C
and is effortlessly integrated into the Go tools.

Go's use of channels for synchronization present interesting opportunities. Go
also supports traditional locking, so we could adopt locking strategies similar
to our other bindings, but we should investigate the Go-like alternatives. There
are anaolgies between Go channels and AMQP links that we will probably exploit.

## State of the implementation

So far we have a wrapper for `pn_url_t` with unit tests and docs. This just
gives us an initial workspace and exercies for the tools and establishes the
basics of using cgo to call into proton code.

Here are some things you can do. First set your environment:

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

    go test apache.org/proton

All feedback most gratefully received especially from experience Go programmers
on issues of style, use of the language or use of the tools.
