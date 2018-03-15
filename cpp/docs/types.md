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

## Holder types

`proton::message::body()` and other message-related data can contain
different types of data at runtime. There are two "holder" types
provided to hold runtime typed data:

 - `proton::scalar` can hold a scalar value of any type.
 - `proton::value` can hold any AMQP value, scalar or compound.

You can set the value in a holder by assignment, and use the
`proton::get()` and `proton::coerce()` templates to extract data in a
type-safe way. Holders also provide functions to query the type of
value they contain.

## Compound types

C++ type            | AMQP type_id         | Description
--------------------|----------------------|-----------------------
See below           | proton::ARRAY        | Sequence of values of the same type
See below           | proton::LIST         | Sequence of values of mixed types
See below           | proton::MAP          | Map of key-value pairs

A `proton::value` containing a `proton::ARRAY` can convert to and from
C++ sequences of the corresponding C++ type: `std::vector`,
`std::deque`, `std::list`, and `std::forward_list`.

`proton::LIST` converts to and from sequences of `proton::value` or
`proton::scalar`, which can hold mixed types of data.

`proton::MAP` converts to and from `std::map`, `std::unordered_map`,
and sequences of `std::pair`.

For example, you can decode a message body with any AMQP map as
follows.

    proton::message m = ...;
    std::map<proton::value, proton::value> map;
    proton::get(m.body(), map);

You can encode a message body with a map of string keys and `uint64_t`
values like this:

    std::unordered_map<std::string, uint64_t> map;
    map["foo"] = 123;
    m.body() = map;

## Include files

`proton/types.hpp` includes all available type definitions and
conversions. Alternatively, you can selectively include the `.hpp`
files you want.
