Proton Documentation            {#index}
====================

## The Protocol Engine

The [Engine API](@ref engine) is a "pure AMQP" toolkit, it decodes AMQP bytes
into proton [events](@ref event) and generates AMQP bytes from application
calls.

The [connection engine](@ref connection_engine) provides a simple bytes in/bytes
out, event-driven interface so you can read AMQP data from any source, process
the resulting [events](@ref event) and write AMQP output to any destination.

There is no IO or threading code in this part of the library, so it can be
embedded in many different environments. The proton project provides language
bindings (Python, Ruby, Go etc.) that embed it into the standard IO and
threading facilities of the bound language.

## Integrating with IO

The [Proactor API](@ref proactor) is a pro-active, asynchronous framewokr to
build single or multi-threaded Proton C applications.

For advanced use-cases it is possible to write your own implementation of the
proactor API for an unusual IO or threading framework. Any proton application
written to the proactor API will be able to use your implementation.

## Messenger and Reactor APIs

The [Messenger](@ref messenger) [Reactor](@ref reactor) APIs were intended
to be simple APIs that included IO support directly out of the box.

They both had good points but were both based on the assumption of a single-threaded
environment using a POSIX-like poll() call. This was a problem for performance on some
platforms and did not support multi-threaded applications.

Note however that application code which interacts with the AMQP @ref engine and
processes AMQP @ref "events" event is the same for the proactor and reactor APIs,
so is quite easy to convert. The main difference is in how connections are set up.
