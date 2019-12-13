#################
Module ``proton``
#################

.. currentmodule:: proton

Module Summary
##############

|

+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`AnnotationDict`    | A dictionary that only takes :class:`symbol` or :class:`ulong` types as a key.                  |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Condition`         | An AMQP Condition object.                                                                       |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Connection`        | A representation of an AMQP connection.                                                         |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Data`              | Provides an interface for decoding, extracting, creating, and encoding arbitrary AMQP data.     |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Delivery`          | Tracks and/or records the delivery of a message over a link.                                    |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Disposition`       | A delivery state.                                                                               |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Endpoint`          | Abstract class from which :class:`Connection`, :class:`Session` and :class:`Link` are derived,  |
|                            | and which defines the state of these classes.                                                   |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Event`             | Notification of a state change in the protocol engine.                                          |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`EventType`         | Connects an event number to an event name, and is used internally by :class:`Event` to represent|
|                            | all known event types.                                                                          |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Link`              | A representation of an AMQP link (a unidirectional channel for transferring messages), of which |
|                            | there are two concrete implementations, :class:`Sender` and :class:`Receiver`.                  |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Message`           | A mutable holder of message content.                                                            |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`PropertyDict`      | A dictionary that only takes :class:`symbol` types as a key.                                    |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Receiver`          | A link over which messages are received.                                                        |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`SASL`              | The SASL layer is responsible for establishing an authenticated and/or encrypted tunnel over    |
|                            | which AMQP frames are passed between peers.                                                     |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Sender`            | A link over which messages are sent.                                                            |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Session`           | A container of links.                                                                           |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`SSL`               | An SSL session associated with a transport.                                                     |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`SSLDomain`         | An SSL configuration domain, used to hold the SSL configuration for one or more SSL sessions.   |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`SSLSessionDetails` | Unique identifier for the SSL session.                                                          |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`SymbolList`        | A list that can only hold :class:`symbol` elements.                                             |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Terminus`          | A source or target for messages.                                                                |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Transport`         | A network channel supporting an AMQP connection.                                                |
+----------------------------+-------------------------------------------------------------------------------------------------+
| :class:`Url`               | **DEPRECATED** Simple URL parser/constructor.                                                   |
+----------------------------+-------------------------------------------------------------------------------------------------+

|

Exceptions
==========

|

+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`ConnectionException` | An exception class raised when exceptions or errors related to a connection arise.      |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`DataException`       | The DataException class is the root of the Data exception hierarchy.                    |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`LinkException`       | An exception class raised when exceptions or errors related to a link arise.            |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`MessageException`    | The MessageException class is the root of the message exception hierarchy.              |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`ProtonException`     | The root of the proton exception hierarchy.                                             |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`SessionException`    | An exception class raised when exceptions or errors related to a session arise.         |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`SSLUnavailable`      | An exception class raised when exceptions or errors related to SSL availability arise.  |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`SSLException`        | An exception class raised when exceptions or errors related to SSL usage arise.         |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`Timeout`             | A timeout exception indicates that a blocking operation has timed out.                  |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`Interrupt`           | An interrupt exception indicates that a blocking operation was interrupted.             |
+------------------------------+-----------------------------------------------------------------------------------------+
| :class:`TransportException`  | An exception class raised when exceptions or errors related to the AMQP transport arise.|
+------------------------------+-----------------------------------------------------------------------------------------+

|

AMQP Types
==========
**NOTE:** Some AMQP types are represented by native Python types. This table contains only classes for non-native
Python types defined in this module. See :ref:`types` for a full list of AMQP types.

|

+---------------------+------------------------------------------------------------+
| :class:`Array`      | An AMQP array, a sequence of AMQP values of a single type. |
+---------------------+------------------------------------------------------------+
| :class:`byte`       | The byte AMQP type.                                        |
+---------------------+------------------------------------------------------------+
| :class:`char`       | The char AMQP type.                                        |
+---------------------+------------------------------------------------------------+
| :class:`Described`  | A described AMQP type.                                     |
+---------------------+------------------------------------------------------------+
| :class:`decimal32`  | The decimal32 AMQP type.                                   |
+---------------------+------------------------------------------------------------+
| :class:`decimal64`  | The decimal64 AMQP type.                                   |
+---------------------+------------------------------------------------------------+
| :class:`decimal128` | The decimal128 AMQP type.                                  |
+---------------------+------------------------------------------------------------+
| :class:`float32`    | The float AMQP type.                                       |
+---------------------+------------------------------------------------------------+
| :class:`int32`      | The signed int AMQP type.                                  |
+---------------------+------------------------------------------------------------+
| :class:`short`      | The short AMQP type.                                       |
+---------------------+------------------------------------------------------------+
| :class:`symbol`     | The symbol AMQP type.                                      |
+---------------------+------------------------------------------------------------+
| :class:`timestamp`  | The timestamp AMQP type.                                   |
+---------------------+------------------------------------------------------------+
| :class:`ubyte`      | The unsigned byte AMQP type.                               |
+---------------------+------------------------------------------------------------+
| :class:`uint`       | The unsigned int AMQP type.                                |
+---------------------+------------------------------------------------------------+
| :class:`ulong`      | The ulong AMQP type.                                       |
+---------------------+------------------------------------------------------------+
| :class:`ushort`     | The unsigned short AMQP type.                              |
+---------------------+------------------------------------------------------------+

|

Module Detail
#############
.. The following classes in the __all__ list are excluded (blacklisted):
   * Collector


.. autoclass:: proton.AnnotationDict
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Condition
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Connection
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: collect, wrap

------------

.. autoclass:: proton.ConnectionException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Data
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: get_mappings, lookup, put_mappings

------------

.. autoclass:: proton.DataException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Delivery
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.Disposition
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Described
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Endpoint
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Event
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.EventType
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Link
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.LinkException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Message
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: decode, encode

------------

.. autoclass:: proton.MessageException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.ProtonException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.PropertyDict
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Receiver
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.SASL
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Sender
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.Session
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.SessionException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.SSL
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.SSLDomain
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.SSLSessionDetails
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.SSLUnavailable
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.SSLException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.SymbolList
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Terminus
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.Timeout
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Interrupt
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Transport
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: wrap

------------

.. autoclass:: proton.TransportException
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args

------------

.. autoclass:: proton.Url
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.Array
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.byte
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.char
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.decimal32
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.decimal64
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.decimal128
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.float32
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.int32
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.short
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.symbol
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.timestamp
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.ubyte
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.uint
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.ulong
    :members:
    :show-inheritance:
    :undoc-members:

------------

.. autoclass:: proton.ushort
    :members:
    :show-inheritance:
    :undoc-members:
