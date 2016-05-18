# IO integration {#io_page}

**Experimental**

The `proton::io` namespace contains a service provider interface (SPI) that
allows you to implement the @ref proton API over alternative IO or threading
libraries.

The `proton::io::connection_engine` converts an AMQP-encoded byte stream, read
from any IO source, into `proton::messaging_handler` calls. It generates an
AMQP-encoded byte stream as output, that can be written to any IO destination.

The connection_engine can be used stand-alone, as an AMQP translator or you
can implement the following two interfaces to provide a complete implementation
of the proton API, that can run any proton application:

 - `proton::container` lets the user initiate or listen for connections.

 - `proton::event_loop` lets the user serialize work with a connection.

@see @ref mt/epoll\_container.cpp for an example.
