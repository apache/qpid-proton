# IO integration {#io_page}

**Experimental**

The proton::io namespace contains a low-level service provider
interface (SPI) that can be used to implement the proton API over any
native or third-party IO library.

The proton::io::connection_engine is the core engine that converts raw
AMQP bytes read from any IO source into proton::messaging_handler
event calls and generates AMQP byte-encoded output that can be written
to any IO destination.

Integrations need to implement two user-visible interfaces:

 - proton::container lets the user initiate or listen for connections.

 - proton::event_loop lets the user serialize their own work with a
   connection.

@see @ref mt/epoll\_container.cpp for an example of an integration.
