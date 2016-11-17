Proton Documentation            {#index}
====================

The @ref engine is a collection of types and functions representing AMQP
concepts.  Together they form a "protocol engine" API to create AMQP connections
and links, handle AMQP @ref event "events" and send and receive messages.

The @ref proactor is a portable, proactive, asynchronous API for single or
multi-threaded applications. It associates the @ref engine API with network
connections (@ref transport "transports") and allows one or more threads to
handle @ref event "events".

**Low-level integration**: The @ref connection\_driver provides a low-level SPI
to feed byte-streams from any source to the @ref engine. You can use it to
integrate proton directly with a foreign event loop or IO library, or to
implement your own @ref proactor to transparently replace proton's IO layer.

**Old APIs**: The @ref messenger and @ref reactor APIs are
older APIs that were limited to single-threaded applications.
@ref reactor applications can be converted to use the @ref proactor since
most of the code is written to the common @ref engine API.
