Proton Documentation            {#index}
====================

The @ref engine is an AMQP "protocol engine".  It provides functions to
manipulate AMQP endpoints and messages and generates [events](@ref event) for
the application to handle.  The @ref engine has no dependencies on IO or
threading libraries.

The @ref proactor is a proactive, asynchronous framework to
build single or multi-threaded Proton C applications. It manages the IO
transport layer so you can write portable, event-driven AMQP code using the
@ref engine API.

**Low-level integration**: The @ref connection_driver provides
a simple bytes in/bytes out interface to the @ref engine for a single
connection.  You can use this to integrate proton with special IO libraries or
external event loops. It is also possible to write your own implementation of the
@ref proactor if you want to transparently replace proton's IO layer.

**Old APIs**: The @ref messenger and @ref reactor APIs are
older APIs that were limited to single-threaded applications.
@ref reactor applications can be converted to use the @ref proactor since
most of the code is written to the common @ref engine API.
