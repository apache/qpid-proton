# AMQP and C++ types {#types_page}

An AMQP message body can hold binary data using any encoding you
like. AMQP also defines its own encoding and types. The AMQP encoding
is often used in message bodies because it is supported by AMQP
libraries on many languages and platforms. You also need to use the
AMQP types to set and examine message properties.

## Scalar types

Each type is identified by a proton::type_id.

C++ type            | AMQP type_id         | Description
--------------------|----------------------|-----------------------
bool                | proton::BOOLEAN      | Boolean true or false
uint8_t             | proton::UBYTE        | 8-bit unsigned byte
int8_t              | proton::BYTE         | 8-bit signed byte
uint16_t            | proton::USHORT       | 16-bit unsigned integer
int16_t             | proton::SHORT        | 16-bit signed integer
uint32_t            | proton::UINT         | 32-bit unsigned integer
int32_t             | proton::INT          | 32-bit signed integer
uint64_t            | proton::ULONG        | 64-bit unsigned integer
int64_t             | proton::LONG         | 64-bit signed integer
wchar_t             | proton::CHAR         | 32-bit unicode code point
float               | proton::FLOAT        | 32-bit binary floating point
double              | proton::DOUBLE       | 64-bit binary floating point
proton::timestamp   | proton::TIMESTAMP    | 64-bit signed milliseconds since 00:00:00 (UTC), 1 January 1970.
proton::decimal32   | proton::DECIMAL32    | 32-bit decimal floating point
proton::decimal64   | proton::DECIMAL64    | 64-bit decimal floating point
proton::decimal128  | proton::DECIMAL128   | 128-bit decimal floating point
proton::uuid        | proton::UUID         | 128-bit universally-unique identifier
std::string         | proton::STRING       | UTF-8 encoded Unicode string
proton::symbol      | proton::SYMBOL       | 7-bit ASCII encoded string
proton::binary      | proton::BINARY       | Variable-length binary data

proton::scalar is a holder that can accept a scalar value of any type.

## Compound types

C++ type            | AMQP type_id         | Description
--------------------|----------------------|-----------------------
See below           | proton::ARRAY        | Sequence of values of the same type
See below           | proton::LIST         | Sequence of values of mixed types
See below           | proton::MAP          | Map of key-value pairs

proton::value is a holder that can accept any AMQP value, scalar or
compound.

proton::ARRAY converts to and from C++ sequences: std::vector,
std::deque, std::list, and std::forward_list.

proton::LIST converts to and from sequences of proton::value or
proton::scalar, which can hold mixed types of data.

proton::MAP converts to and from std::map, std::unordered_map, and
sequences of std::pair.

When decoding, the encoded map types must be convertible to the element type of the
C++ sequence or the key-value types of the C++ map.  Use proton::value as the
element or key-value type to decode any ARRAY, LIST, or MAP.

For example you can decode any AMQP MAP into:

    std::map<proton::value, proton::value>

You can decode any AMQP LIST or ARRAY into:

    std::vector<proton::value>

## Include files

You can simply include proton/types.hpp to get all the type
definitions and conversions. Alternatively, you can selectively
include only what you need:

 - Include proton/types_fwd.hpp: forward declarations for all types.
 - Include individual `.hpp` files as per the table above.
