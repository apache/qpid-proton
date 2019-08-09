#################
Module ``proton``
#################

.. currentmodule:: proton

Module Summary
##############

|

.. autosummary::

    Condition
    Connection
    Data
    Delivery
    Disposition
    Endpoint
    Event
    EventType
    Link
    Message
    Receiver
    SASL
    Sender
    Session
    SSL
    SSLDomain
    SSLSessionDetails
    Terminus
    Transport
    Url

|

Exceptions
==========

|

.. autosummary::

    ConnectionException
    DataException
    LinkException
    MessageException
    ProtonException
    SessionException
    SSLUnavailable
    SSLException
    Timeout
    Interrupt
    TransportException

|

AMQP Types
==========

|

.. autosummary::
    :nosignatures:

    Array
    byte
    char
    Described
    decimal32
    decimal64
    decimal128
    float32
    int32
    short
    symbol
    timestamp
    ubyte
    uint
    ulong
    ushort

|

Module Detail
#############
.. The following classes in the __all__ list are excluded (blacklisted):
   * Collector

|

.. autoclass:: proton.Array
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
