.. _types:

##########
AMQP Types
##########

These tables summarize the various AMQP types and their Python API equivalents as used in the API.

|

============
Scalar Types
============

|

+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| AMQP Type  | Proton C API Type | Proton Python API Type    | Description                                                            |
+============+===================+===========================+========================================================================+
| null       | PN_NONE           | ``None``                  | Indicates an empty value.                                              |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| boolean    | PN_BOOL           | ``bool``                  | Represents a true or false value.                                      |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| ubyte      | PN_UBYTE          | :class:`proton.ubyte`     | Integer in the range :math:`0` to :math:`2^8 - 1` inclusive.           |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| byte       | PN_BYTE           | :class:`proton.byte`      | Integer in the range :math:`-(2^7)` to :math:`2^7 - 1` inclusive.      |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| ushort     | PN_USHORT         | :class:`proton.ushort`    | Integer in the range :math:`0` to :math:`2^{16} - 1` inclusive.        |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| short      | PN_SHORT          | :class:`proton.short`     | Integer in the range :math:`-(2^{15})` to :math:`2^{15} - 1` inclusive.|
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| uint       | PN_UINT           | :class:`proton.uint`      | Integer in the range :math:`0` to :math:`2^{32} - 1` inclusive.        |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| int        | PN_INT            | :class:`proton.int32`     | Integer in the range :math:`-(2^{31})` to :math:`2^{31} - 1` inclusive.|
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| char       | PN_CHAR           | :class:`proton.char`      | A single Unicode character.                                            |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| ulong      | PN_ULONG          | :class:`proton.ulong`     | Integer in the range :math:`0` to :math:`2^{64} - 1` inclusive.        |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| long       | PN_LONG           | ``int`` or ``long``       | Integer in the range :math:`-(2^{63})` to :math:`2^{63} - 1` inclusive.|
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| timestamp  | PN_TIMESTAMP      | :class:`proton.timestamp` | An absolute point in time with millisecond precision.                  |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| float      | PN_FLOAT          | :class:`proton.float32`   | 32-bit floating point number (IEEE 754-2008 binary32).                 |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| double     | PN_DOUBLE         | ``double``                | 64-bit floating point number (IEEE 754-2008 binary64).                 |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| decimal32  | PN_DECIMAL32      | :class:`proton.decimal32` | 32-bit decimal number (IEEE 754-2008 decimal32).                       |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| decimal64  | PN_DECIMAL64      | :class:`proton.decimal64` | 64-bit decimal number (IEEE 754-2008 decimal64).                       |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| decimal128 | PN_DECIMAL128     | :class:`proton.decimal128`| 128-bit decimal number (IEEE 754-2008 decimal128).                     |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| uuid       | PN_UUID           | ``uuid.UUID``             | A universally unique identifier as defined by RFC-4122 section 4.1.2.  |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| binary     | PN_BINARY         | ``bytes``                 | A sequence of octets.                                                  |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| string     | PN_STRING         | ``str``                   | A sequence of Unicode characters.                                      |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+
| symbol     | PN_SYMBOL         | :class:`proton.symbol`    | Symbolic values from a constrained domain.                             |
+------------+-------------------+---------------------------+------------------------------------------------------------------------+

|

==============
Compound Types
==============

|

+-----------+-------------------+---------------------------+-----------------------------------------------------+
| AMQP Type | Proton C API Type | Proton Python API Type    | Description                                         |
+===========+===================+===========================+=====================================================+
| array     | PN_ARRAY          | :class:`proton.Array`     | A sequence of values of a single type.              |
+-----------+-------------------+---------------------------+-----------------------------------------------------+
| list      | PN_LIST           | ``list``                  | A sequence of polymorphic values.                   |
+-----------+-------------------+---------------------------+-----------------------------------------------------+
| map       | PN_MAP            | ``dict``                  | A polymorphic mapping from distinct keys to values. |
+-----------+-------------------+---------------------------+-----------------------------------------------------+

|

=================
Specialized Types
=================

The following classes implement specialized or restricted types to help
enforce type restrictions in the AMQP specification.

|

+-------------------------------+------------------------------------------------------------------------------------+------------------------------------------------+
| Proton Python API Type        | Description                                                                        | Where used in API                              |
+===============================+====================================================================================+================================================+
| :class:`proton.SymbolList`    | A ``list`` that only accepts :class:`proton.symbol` elements. However, will        | :attr:`proton.Connection.desired_capabilities` |
|                               | silently convert strings to symbols.                                               | :attr:`proton.Connection.offered_capabilities` |
+-------------------------------+------------------------------------------------------------------------------------+------------------------------------------------+
| :class:`proton.PropertyDict`  | A ``dict`` that only accppts :class:`proton.symbol` keys. However, will silently   | :attr:`proton.Connection.properties`           |
|                               | convert strings to symbols.                                                        |                                                |
+-------------------------------+------------------------------------------------------------------------------------+------------------------------------------------+
| :class:`proton.AnnotationDict`| A ``dict`` that only accppts :class:`proton.symbol` or :class:`proton.ulong` keys. | :attr:`proton.Message.annotations`             |
|                               | However, will silently convert strings to symbols.                                 | :attr:`proton.Message.instructions`            |
+-------------------------------+------------------------------------------------------------------------------------+------------------------------------------------+

|

These types would typically be used where the the above attributes are set. They will silently convert strings to symbols,
but will raise an error if a not-allowed type is used. For example:

        >>> from proton import symbol, ulong, Message, AnnotationDict
        >>> msg = Message()
        >>> msg.annotations = AnnotationDict({'one':1, symbol('two'):2, ulong(3):'three'})
        >>> msg.annotations
        AnnotationDict({symbol('one'): 1, symbol('two'): 2, ulong(3): 'three'})
        >>> m.instructions = AnnotationDict({'one':1, symbol('two'):2, ulong(3):'three', 4:'four'})
          ...
        KeyError: "invalid non-symbol key: <class 'int'>: 4"
        >>> m.instructions
        >>> 

