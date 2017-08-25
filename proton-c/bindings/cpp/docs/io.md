# IO integration {#io_page}

**Experimental** - The `proton::io` interfaces are new and remain
subject to change.

The `proton::io` namespace contains a service provider interface (SPI)
that allows you to implement the Proton API over alternative IO or
threading libraries.

The `proton::io::connection_driver` class converts an AMQP-encoded
byte stream, read from any IO source, into `proton::messaging_handler`
calls. It generates an AMQP-encoded byte stream as output that can be
written to any IO destination.

The connection driver is deliberately very simple and low level. It
performs no IO of its own, no thread-related locking, and is written
in simple C++98-compatible code.



