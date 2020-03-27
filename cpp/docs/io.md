# IO integration {#io_page}

**Unsettled API** - The `proton::io` interfaces are new and remain
subject to change.

The `proton::io` namespace contains a service provider interface (SPI)
that allows you to embed Proton in alternative IO or threading
libraries.

The `proton::io::connection_driver` class converts an AMQP-encoded
byte stream, read from any IO source, into `proton::messaging_handler`
calls. It generates an AMQP-encoded byte stream as output that can be
written to any IO destination.

The connection driver has no threading or IO dependencies. It is not
thread-safe, but separate instances are independent and can be run
concurrently in a multithreaded framework. The driver is written in
portable C++98-compatible code.

For examples of use, see
[the proton source code](https://qpid.apache.org/proton), in particular the
C++ `proton::container`.
