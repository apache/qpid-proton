Proton's Engine is a stateful component with a low-level API that allows an
application to communicate using AMQP. This document gives a high level overview
of the Engine's design, intended to be read by application developers intending
to use it.

The Engine is built around the concept of a protocol engine. The idea behind a
protocol engine is to capture all the complex details of implementing a given
protocol in a way that is as decoupled as possible from OS details such as I/O
and threading models. The result is a highly portable and easily embedded
component that provides a full protocol implementation. 


The Engine API
--------------

The Engine contains in-memory representations of AMQP entities such as
Connection, Session and Delivery. These are manipulated via its API, which
consists of two main parts.

- The *control and query API*, commonly referred to as *The Top Half*, which
  offers functions to directly create, modify and query the Connections,
  Sessions etc.

- The *transport API*, commonly referred to as *The Bottom Half*, which contains
  a small set of functions to operate on the AMQP entities therein by accepting
  binary AMQP input and producing binary AMQP output. The Engine's transport
  layer can be thought of as transforming a *queue of bytes*, therefore the API
  is expressed in terms of input appended to the *tail* and output fetched from
  the *head*.


Typical Engine usage
--------------------

The diagram below shows how the Engine is typically used by an application.  The
socket's remote peer is serviced by another AMQP application, which may (or may
not) use Proton.

<pre>
<![CDATA[

                              +------------ +          +---------------+
                              |             |          |               |
                              | Application |--------->| Engine        |
                              | business    |          | "Top Half"    |
                              | logic       |          | Control and   |
                              |             |<---------| query API     |
                              |             |          |               |
                              |             |          +---------------+
                              |             |                 |
     +-------------+          +-------------+          +---------------+
     |             |  Input   |             |  Tail    | Engine        |
     |             |--------->|             |--------->| "Bottom half" |
     |   Socket    |          | Application |          | Transport API |
     |             |<---------| I/O layer   |<---------|               |
     |             |  Output  |             |  Head    |               |
     +-------------+          +-------------+          +---------------+
]]>
</pre>

For maximum flexibility, the Engine is not multi-threaded. It is therefore
typical for an application thread to loop continuously, repeatedly calling the
Top Half and Bottom Half functions.


Implementations
---------------

Implementations of the Engine currently exist in C and Java. Bindings exist from
several languages (e.g. Ruby, Python, Java Native Interface) to the C Engine.

For more information see the documentation in the code.


