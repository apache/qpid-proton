# Introduction {#mainpage}

The Qpid Proton C++ API enables writing clients and servers that send
and receive messages using the AMQP protocol. It is part of the
[Qpid Proton](https://qpid.apache.org/proton/index.html) suite of
messaging APIs.

The @ref overview_page presents the API's central concepts and
mechanics.

The @ref tutorial_page guides you through some basic examples.  See
[**Examples**](examples.html) for a complete list of the sample
programs, including more advanced ones.

Qpid Proton C++ can be used in single- and multithreaded applications.
See @ref mt_page for guidance on writing efficient multithreaded
messaging applications.

## Namespaces

The main @ref proton namespace contains classes and functions
representing AMQP concepts and key elements of the API.  Together they
form a "protocol engine" API to create AMQP @ref proton::connection
"connections" and @ref proton::link "links", handle @ref
proton::messaging\_handler "events", and send and receive @ref
proton::message "messages".  See @ref overview_page for more
information.

The main @ref proton namespace also contains C++ classes and functions
for handling AMQP- and API-specific data types. See @ref types_page
for more information.

The @ref proton::codec namespace contains interfaces for AMQP data
encoding and decoding.

The @ref proton::io namespace contains interfaces for integrating with
platform-native network IO.  See @ref io_page for more information.

## Conventions

Elements of the API marked as **Unsettled API**, including any
elements contained within them, are still evolving and thus are
subject to change.  They are available to use, but newer versions of
Proton may require changes to your application source code.

Elements marked **Deprecated** are slated for removal in a future
release.

Sections labeled **Thread safety** describe when and where it is safe
to call functions or access data across threads.

Sections called **C++ versions** discuss features of the API that
depend on particular versions of C++ such as C++11.

## URLs

The API uses URLs to identify three different kinds of resources.  All
URL argument names are suffixed with `_url`.

Connection URLs (`conn_url` in argument lists) specify a target for
outgoing network connections.  The path part of the URL is ignored.

Address URLs (`addr_url`) extend the connection URL to reference an
AMQP node such as a queue or topic.  The path of the URL, minus the
leading slash, is treated as the AMQP address of the node.

Listener URLs (`listen_url`) specify a local network address and port for
accepting incoming TCP connections.  The path part of the URL is ignored.
The host part may be empty, meaning "listen on all available interfaces".
