Proton Documentation            {#index}
====================

## The Protocol Engine

The [Engine API](@ref engine) is a "pure AMQP" toolkit, it decodes AMQP bytes
into proton [events](@ref event) and generates AMQP bytes from application
calls. There is no IO or threading code in this part of the library.

## Proactive event-driven programming

The [Proactor API](@ref proactor) is a pro-active, asynchronous framework to
build single or multi-threaded Proton C applications. It manages the IO
transport layer so you can write portable, event-driven AMQP code using the @ref
engine API.

## IO Integration

The [connection driver](@ref connection_driver) provides a simple bytes in/bytes
out, event-driven interface so you can read AMQP data from any source, process
the resulting [events](@ref event) and write AMQP output to any destination. It
lets you use proton in in alternate event loops, or for specialized embedded
applications.

It is also possible to write your own implementation of the @ref proactor if you
are dealing with an unusual IO or threading framework. Any proton application
written to the proactor API will be able to use your implementation.

## Messenger and Reactor APIs (deprecated)

The [Messenger](@ref messenger) [Reactor](@ref reactor) APIs are older APIs
that were limited to single-threaded applications.

Existing @ref reactor applications can be converted easily to use the @ref proactor,
since they share the same @engine API and @ref event set.
