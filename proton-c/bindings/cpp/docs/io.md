# IO integration {#io_page}

**Experimental** - The `proton::io` interfaces are new and remain
subject to change.

The `proton::io` namespace contains a service provider interface (SPI)
that allows you to implement the Proton API over alternative IO or
threading libraries.

The `proton::io::connection_engine` class converts an AMQP-encoded
byte stream, read from any IO source, into `proton::messaging_handler`
calls. It generates an AMQP-encoded byte stream as output that can be
written to any IO destination.

The connection engine is deliberately very simple and low level. It
performs no IO of its own, no thread-related locking, and is written
in simple C++98-compatible code.

The connection engine can be used standalone as an AMQP translator, or
you can implement the following two interfaces to provide a complete
implementation of the Proton API that can run any Proton application:

 - `proton::container` lets the user initiate or listen for connections.
 - `proton::event_loop` lets the user serialize work with respect to a
   connection.

@see @ref mt/epoll\_container.cpp for an example.
