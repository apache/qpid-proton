# Introduction {#index}

This is the C API for the Proton AMQP protocol engine. It allows you
to write client and server applications that send and receive AMQP
messages.

## Core

@ref core is a collection of types and functions representing AMQP
concepts.  Together they form a "protocol engine" API to create AMQP
connections and links, handle @ref event "events", and send and
receive @ref message "messages".

## Types

@ref types contains C data types, including @ref amqp\_types and @ref
api\_types.

## Codec

@ref codec has functions for AMQP data encoding and decoding.

## IO

@ref io holds interfaces for integrating Proton with platform IO.

The @ref proactor is a portable, proactive, asynchronous API for
single- or multithreaded applications. It associates AMQP @ref
connection "connections" with network connections (@ref transport
"transports") and allows one or more threads to handle @ref event
"events".

**Low-level integration** - The @ref connection\_driver provides a
low-level SPI to feed byte streams from any source to the protocol
engine. You can use it to integrate Proton directly with a foreign
event loop or IO library, or to implement your own @ref proactor to
transparently replace Proton's IO layer.
