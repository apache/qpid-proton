# Introduction {#mainpage}

The Qpid Proton C API enables writing clients and servers that send
and receive messages using the AMQP protocol. It is part of the
[Qpid Proton](https://qpid.apache.org/proton/index.html) suite of
messaging APIs.

## Modules

The @ref core module is a collection of types and functions
representing AMQP concepts and key elements of the API.  Together they
form a "protocol engine" API to create AMQP @ref connection
"connections" and @ref link "links", handle @ref event "events", and
send and receive @ref message "messages".

The @ref types module contains C types and functions for handling
AMQP- and API-specific data types.

The @ref codec module has functions for AMQP data encoding and
decoding.

The @ref io module contains interfaces for integrating with
platform-native network IO.  See @ref io_page for more information.

## Conventions

Elements of the API marked as **Unsettled API**, including any
elements contained within them, are still evolving and thus are
subject to change.  They are available to use, but newer versions of
Proton may require changes to your application source code.

Elements marked **Deprecated** are slated for removal in a future
release.
