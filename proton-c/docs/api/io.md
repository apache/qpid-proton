## IO integration {#io_page}

**Unsettled API** - The IO interfaces are new and remain subject to
change.

The @ref proactor is a portable, proactive, asynchronous API for
single- or multithreaded applications. It associates AMQP @ref
connection "connections" with network connections (@ref transport
"transports") and allows one or more threads to handle @ref event
"events".

The @ref connection\_driver is a low-level SPI to feed byte streams
from any source to the protocol engine. You can use it to integrate
Proton directly with a foreign event loop or IO library, or to
implement your own @ref proactor to transparently replace Proton's IO
layer.
