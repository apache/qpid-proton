# *EXPERIMENTAL* Go examples for proton.

See ../../proton-c/bindings/go/README.md

These are sketches of examples for the experimental Go binding.

They don't have sufficient error handling.

They compile and run but do nothing as the binding implementation is just stubs right now.

- listen.go: listens on a port, prints any messages it receives.
- send.go: sends command-line arguments as messages with string bodies.

You can use the two together, e.g. to listen/send on port 1234:

go run listen.go -addr :1234
go run send.go -addr :1234 foo bar 
