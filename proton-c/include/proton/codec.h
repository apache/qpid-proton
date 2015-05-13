#ifndef PROTON_CODEC_H
#define PROTON_CODEC_H 1

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <proton/import_export.h>
#include <proton/object.h>
#include <proton/types.h>
#include <proton/error.h>
#include <proton/type_compat.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * Data API for proton.
 *
 * @defgroup data Data
 * @{
 */

/**
 * Identifies an AMQP type.
 */
typedef enum {

  /**
   * The NULL AMQP type.
   */
  PN_NULL = 1,

  /**
   * The boolean AMQP type.
   */
  PN_BOOL = 2,

  /**
   * The unsigned byte AMQP type. An 8 bit unsigned integer.
   */
  PN_UBYTE = 3,

  /**
   * The byte AMQP type. An 8 bit signed integer.
   */
  PN_BYTE = 4,

  /**
   * The unsigned short AMQP type. A 16 bit unsigned integer.
   */
  PN_USHORT = 5,

  /**
   * The short AMQP type. A 16 bit signed integer.
   */
  PN_SHORT = 6,

  /**
   * The unsigned int AMQP type. A 32 bit unsigned integer.
   */
  PN_UINT = 7,

  /**
   * The signed int AMQP type. A 32 bit signed integer.
   */
  PN_INT = 8,

  /**
   * The char AMQP type. A 32 bit unicode character.
   */
  PN_CHAR = 9,

  /**
   * The ulong AMQP type. An unsigned 32 bit integer.
   */
  PN_ULONG = 10,

  /**
   * The long AMQP type. A signed 32 bit integer.
   */
  PN_LONG = 11,

  /**
   * The timestamp AMQP type. A signed 64 bit value measuring
   * milliseconds since the epoch.
   */
  PN_TIMESTAMP = 12,

  /**
   * The float AMQP type. A 32 bit floating point value.
   */
  PN_FLOAT = 13,

  /**
   * The double AMQP type. A 64 bit floating point value.
   */
  PN_DOUBLE = 14,

  /**
   * The decimal32 AMQP type. A 32 bit decimal floating point value.
   */
  PN_DECIMAL32 = 15,

  /**
   * The decimal64 AMQP type. A 64 bit decimal floating point value.
   */
  PN_DECIMAL64 = 16,

  /**
   * The decimal128 AMQP type. A 128 bit decimal floating point value.
   */
  PN_DECIMAL128 = 17,

  /**
   * The UUID AMQP type. A 16 byte UUID.
   */
  PN_UUID = 18,

  /**
   * The binary AMQP type. A variable length sequence of bytes.
   */
  PN_BINARY = 19,

  /**
   * The string AMQP type. A variable length sequence of unicode
   * characters.
   */
  PN_STRING = 20,

  /**
   * The symbol AMQP type. A variable length sequence of unicode
   * characters.
   */
  PN_SYMBOL = 21,

  /**
   * A described AMQP type.
   */
  PN_DESCRIBED = 22,

  /**
   * An AMQP array. A monomorphic sequence of other AMQP values.
   */
  PN_ARRAY = 23,

  /**
   *  An AMQP list. A polymorphic sequence of other AMQP values.
   */
  PN_LIST = 24,

  /**
   * An AMQP map. A polymorphic container of other AMQP values formed
   * into key/value pairs.
   */
  PN_MAP = 25
} pn_type_t;

/**
 * Return a string name for an AMQP type.
 *
 * @param type an AMQP type
 * @return the string name of the given type
 */
PN_EXTERN const char *pn_type_name(pn_type_t type);

/**
 * A descriminated union that holds any scalar AMQP value. The type
 * field indicates the AMQP type of the value, and the union may be
 * used to access the value for a given type.
 */
typedef struct {
  /**
   * Indicates the type of value the atom is currently pointing to.
   * See ::pn_type_t for details on AMQP types.
   */
  pn_type_t type;
  union {
    /**
     * Valid when type is ::PN_BOOL.
     */
    bool as_bool;

    /**
     * Valid when type is ::PN_UBYTE.
     */
    uint8_t as_ubyte;

    /**
     * Valid when type is ::PN_BYTE.
     */
    int8_t as_byte;

    /**
     * Valid when type is ::PN_USHORT.
     */
    uint16_t as_ushort;

    /**
     * Valid when type is ::PN_SHORT.
     */
    int16_t as_short;

    /**
     * Valid when type is ::PN_UINT.
     */
    uint32_t as_uint;

    /**
     * Valid when type is ::PN_INT.
     */
    int32_t as_int;

    /**
     * Valid when type is ::PN_CHAR.
     */
    pn_char_t as_char;

    /**
     * Valid when type is ::PN_ULONG.
     */
    uint64_t as_ulong;

    /**
     * Valid when type is ::PN_LONG.
     */
    int64_t as_long;

    /**
     * Valid when type is ::PN_TIMESTAMP.
     */
    pn_timestamp_t as_timestamp;

    /**
     * Valid when type is ::PN_FLOAT.
     */
    float as_float;

    /**
     * Valid when type is ::PN_DOUBLE.
     */
    double as_double;

    /**
     * Valid when type is ::PN_DECIMAL32.
     */
    pn_decimal32_t as_decimal32;

    /**
     * Valid when type is ::PN_DECIMAL64.
     */
    pn_decimal64_t as_decimal64;

    /**
     * Valid when type is ::PN_DECIMAL128.
     */
    pn_decimal128_t as_decimal128;

    /**
     * Valid when type is ::PN_UUID.
     */
    pn_uuid_t as_uuid;

    /**
     * Valid when type is ::PN_BINARY or ::PN_STRING or ::PN_SYMBOL.
     * When the type is ::PN_STRING the field will point to utf8
     * encoded unicode. When the type is ::PN_SYMBOL, the field will
     * point to 7-bit ASCII. In the latter two cases, the bytes
     * pointed to are *not* necessarily null terminated.
     */
    pn_bytes_t as_bytes;
  } u;
} pn_atom_t;

/**
 * An AMQP Data object.
 *
 * A pn_data_t object provides an interface for decoding, extracting,
 * creating, and encoding arbitrary AMQP data. A pn_data_t object
 * contains a tree of AMQP values. Leaf nodes in this tree correspond
 * to scalars in the AMQP type system such as @link ::PN_INT ints
 * @endlink or @link ::PN_STRING strings @endlink. Non-leaf nodes in
 * this tree correspond to compound values in the AMQP type system
 * such as @link ::PN_LIST lists @endlink, @link ::PN_MAP maps
 * @endlink, @link ::PN_ARRAY arrays @endlink, or @link ::PN_DESCRIBED
 * described @endlink values. The root node of the tree is the
 * pn_data_t object itself and can have an arbitrary number of
 * children.
 *
 * A pn_data_t object maintains the notion of the current node and the
 * current parent node. Siblings are ordered within their parent.
 * Values are accessed and/or added by using the ::pn_data_next(),
 * ::pn_data_prev(), ::pn_data_enter(), and ::pn_data_exit()
 * operations to navigate to the desired location in the tree and
 * using the supplied variety of pn_data_put_* / pn_data_get_*
 * operations to access or add a value of the desired type.
 *
 * The pn_data_put_* operations will always add a value _after_ the
 * current node in the tree. If the current node has a next sibling
 * the pn_data_put_* operations will overwrite the value on this node.
 * If there is no current node or the current node has no next sibling
 * then one will be added. The pn_data_put_* operations always set the
 * added/modified node to the current node. The pn_data_get_*
 * operations read the value of the current node and do not change
 * which node is current.
 *
 * The following types of scalar values are supported:
 *
 *  - ::PN_NULL
 *  - ::PN_BOOL
 *  - ::PN_UBYTE
 *  - ::PN_USHORT
 *  - ::PN_SHORT
 *  - ::PN_UINT
 *  - ::PN_INT
 *  - ::PN_ULONG
 *  - ::PN_LONG
 *  - ::PN_FLOAT
 *  - ::PN_DOUBLE
 *  - ::PN_BINARY
 *  - ::PN_STRING
 *  - ::PN_SYMBOL
 *
 * The following types of compound values are supported:
 *
 *  - ::PN_DESCRIBED
 *  - ::PN_ARRAY
 *  - ::PN_LIST
 *  - ::PN_MAP
 */
typedef struct pn_data_t pn_data_t;

/**
 * Construct a pn_data_t object with the supplied initial capacity. A
 * pn_data_t will grow automatically as needed, so an initial capacity
 * of 0 is permitted.
 *
 * @param capacity the initial capacity
 * @return the newly constructed pn_data_t
 */
PN_EXTERN pn_data_t *pn_data(size_t capacity);

/**
 * Free a pn_data_t object.
 *
 * @param data a pn_data_t object or NULL
 */
PN_EXTERN void pn_data_free(pn_data_t *data);

/**
 * Access the current error code for a given pn_data_t.
 *
 * @param data a pn_data_t object
 * @return the current error code
 */
PN_EXTERN int pn_data_errno(pn_data_t *data);

/**
 * Access the current error for a givn pn_data_t.
 *
 * Every pn_data_t has an error descriptor that is created with the
 * pn_data_t and dies with the pn_data_t. The error descriptor is
 * updated whenever an operation fails. The ::pn_data_error() function
 * may be used to access a pn_data_t's error descriptor.
 *
 * @param data a pn_data_t object
 * @return a pointer to the pn_data_t's error descriptor
 */
PN_EXTERN pn_error_t *pn_data_error(pn_data_t *data);

PN_EXTERN int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap);
PN_EXTERN int pn_data_fill(pn_data_t *data, const char *fmt, ...);
PN_EXTERN int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap);
PN_EXTERN int pn_data_scan(pn_data_t *data, const char *fmt, ...);

/**
 * Clears a pn_data_t object.
 *
 * A cleared pn_data_t object is equivalent to a newly constructed
 * one.
 *
 * @param data the pn_data_t object to clear
 */
PN_EXTERN void pn_data_clear(pn_data_t *data);

/**
 * Returns the total number of nodes contained in a pn_data_t object.
 * This includes all parents, children, siblings, grandchildren, etc.
 * In other words the count of all ancesters and descendents of the
 * current node, along with the current node if there is one.
 *
 * @param data a pn_data_t object
 * @return the total number of nodes in the pn_data_t object
 */
PN_EXTERN size_t pn_data_size(pn_data_t *data);

/**
 * Clears current node pointer and sets the parent to the root node.
 * Clearing the current node sets it _before_ the first node, calling
 * ::pn_data_next() will advance to the first node.
 */
PN_EXTERN void pn_data_rewind(pn_data_t *data);

/**
 * Advances the current node to its next sibling and returns true. If
 * there is no next sibling the current node remains unchanged and
 * false is returned.
 *
 * @param data a pn_data_t object
 * @return true iff the current node was changed
 */
PN_EXTERN bool pn_data_next(pn_data_t *data);

/**
 * Moves the current node to its previous sibling and returns true. If
 * there is no previous sibling the current node remains unchanged and
 * false is returned.
 *
 * @param data a pn_data_t object
 * @return true iff the current node was changed
 */
PN_EXTERN bool pn_data_prev(pn_data_t *data);

/**
 * Sets the parent node to the current node and clears the current
 * node. Clearing the current node sets it _before_ the first child,
 * calling ::pn_data_next() advances to the first child. This
 * operation will return false if there is no current node or if the
 * current node is not a compound type.
 *
 * @param data a pn_data_object
 * @return true iff the pointers to the current/parent nodes are changed
 */
PN_EXTERN bool pn_data_enter(pn_data_t *data);

/**
 * Sets the current node to the parent node and the parent node to its
 * own parent. This operation will return false if there is no current
 * node or parent node.
 *
 * @param data a pn_data object
 * @return true iff the pointers to the current/parent nodes are
 *         changed
 */
PN_EXTERN bool pn_data_exit(pn_data_t *data);

PN_EXTERN bool pn_data_lookup(pn_data_t *data, const char *name);

/**
 * Access the type of the current node. Returns an undefined value if
 * there is no current node.
 *
 * @param data a data object
 * @return the type of the current node
 */
PN_EXTERN pn_type_t pn_data_type(pn_data_t *data);

/**
 * Prints the contents of a pn_data_t object using ::pn_data_format()
 * to stdout.
 *
 * @param data a pn_data_t object
 * @return zero on success or an error on failure
 */
PN_EXTERN int pn_data_print(pn_data_t *data);

/**
 * Formats the contents of a pn_data_t object in a human readable way
 * and writes them to the indicated location. The size pointer must
 * hold the amount of free space following the bytes pointer, and upon
 * success will be updated to indicate how much space has been used.
 *
 * @param data a pn_data_t object
 * @param bytes a buffer to write the output to
 * @param size a pointer to the size of the buffer
 * @return zero on succes, or an error on failure
 */
PN_EXTERN int pn_data_format(pn_data_t *data, char *bytes, size_t *size);

/**
 * Writes the contents of a data object to the given buffer as an AMQP
 * data stream.
 *
 * @param data the data object to encode
 * @param bytes the buffer for encoded data
 * @param size the size of the buffer
 *
 * @param ssize_t returns the size of the encoded data on success or
 *                an error code on failure
 */
PN_EXTERN ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);

/**
 * Returns the number of bytes needed to encode a data object.
 *
 * @param data the data object
 *
 * @param ssize_t returns the size of the encoded data or an error code if data is invalid.
 */
PN_EXTERN ssize_t pn_data_encoded_size(pn_data_t *data);

/**
 * Decodes a single value from the contents of the AMQP data stream
 * into the current data object. Note that if the pn_data_t object is
 * pointing to a current node, the decoded value will overwrite the
 * current one. If the pn_data_t object has no current node then a
 * node will be appended to the current parent. If there is no current
 * parent then a node will be appended to the pn_data_t itself.
 *
 * Upon success, this operation returns the number of bytes consumed
 * from the AMQP data stream. Upon failure, this operation returns an
 * error code.
 *
 * @param data a pn_data_t object
 * @param bytes a pointer to an encoded AMQP data stream
 * @param size the size of the encoded AMQP data stream
 * @return the number of bytes consumed from the AMQP data stream or an error code
 */
PN_EXTERN ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);

/**
 * Puts an empty list value into a pn_data_t. Elements may be filled
 * by entering the list node using ::pn_data_enter() and using
 * ::pn_data_put_* to add the desired contents. Once done,
 * ::pn_data_exit() may be used to return to the current level in the
 * tree and put more values.
 *
 *   @code
 *   pn_data_t *data = pn_data(0);
 *   ...
 *   pn_data_put_list(data);
 *   pn_data_enter(data);
 *   pn_data_put_int(data, 1);
 *   pn_data_put_int(data, 2);
 *   pn_data_put_int(data, 3);
 *   pn_data_exit(data);
 *   ...
 *   @endcode
 *
 * @param data a pn_data_t object
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_list(pn_data_t *data);

/**
 * Puts an empty map value into a pn_data_t. Elements may be filled by
 * entering the map node and putting alternating key value pairs.
 *
 *   @code
 *   pn_data_t *data = pn_data(0);
 *   ...
 *   pn_data_put_map(data);
 *   pn_data_enter(data);
 *   pn_data_put_string(data, pn_bytes(3, "key"));
 *   pn_data_put_string(data, pn_bytes(5, "value"));
 *   pn_data_exit(data);
 *   ...
 *   @endcode
 *
 * @param data a pn_data_t object
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_map(pn_data_t *data);

/**
 * Puts an empty array value into a pn_data_t. Elements may be filled
 * by entering the array node and putting the element values. The
 * values must all be of the specified array element type. If an array
 * is described then the first child value of the array is the
 * descriptor and may be of any type.
 *
 * @code
 *   pn_data_t *data = pn_data(0);
 *   ...
 *   pn_data_put_array(data, false, PN_INT);
 *   pn_data_enter(data);
 *   pn_data_put_int(data, 1);
 *   pn_data_put_int(data, 2);
 *   pn_data_put_int(data, 3);
 *   pn_data_exit(data);
 *   ...
 *   pn_data_put_array(data, True, Data.DOUBLE);
 *   pn_data_enter(data);
 *   pn_data_put_symbol(data, "array-descriptor");
 *   pn_data_put_double(data, 1.1);
 *   pn_data_put_double(data, 1.2);
 *   pn_data_put_double(data, 1.3);
 *   pn_data_exit(data);
 *   ...
 * @endcode
 *
 * @param data a pn_data_t object
 * @param described specifies whether the array is described
 * @param type the type of the array
 *
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_array(pn_data_t *data, bool described, pn_type_t type);

/**
 * Puts a described value into a pn_data_t object. A described node
 * has two children, the descriptor and the value. These are specified
 * by entering the node and putting the desired values.
 *
 * @code
 *   pn_data_t *data = pn_data(0);
 *   ...
 *   pn_data_put_described(data);
 *   pn_data_enter(data);
 *   pn_data_put_symbol(data, pn_bytes(16, "value-descriptor"));
 *   pn_data_put_string(data, pn_bytes(9, "the value"));
 *   pn_data_exit(data);
 *   ...
 * @endcode
 *
 * @param data a pn_data_t object
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_described(pn_data_t *data);

/**
 * Puts a ::PN_NULL value.
 *
 * @param data a pn_data_t object
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_null(pn_data_t *data);

/**
 * Puts a ::PN_BOOL value.
 *
 * @param data a pn_data_t object
 * @param b the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_bool(pn_data_t *data, bool b);

/**
 * Puts a ::PN_UBYTE value.
 *
 * @param data a pn_data_t object
 * @param ub the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_ubyte(pn_data_t *data, uint8_t ub);

/**
 * Puts a ::PN_BYTE value.
 *
 * @param data a pn_data_t object
 * @param b the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_byte(pn_data_t *data, int8_t b);

/**
 * Puts a ::PN_USHORT value.
 *
 * @param data a pn_data_t object
 * @param us the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_ushort(pn_data_t *data, uint16_t us);

/**
 * Puts a ::PN_SHORT value.
 *
 * @param data a pn_data_t object
 * @param s the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_short(pn_data_t *data, int16_t s);

/**
 * Puts a ::PN_UINT value.
 *
 * @param data a pn_data_t object
 * @param ui the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_uint(pn_data_t *data, uint32_t ui);

/**
 * Puts a ::PN_INT value.
 *
 * @param data a pn_data_t object
 * @param i the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_int(pn_data_t *data, int32_t i);

/**
 * Puts a ::PN_CHAR value.
 *
 * @param data a pn_data_t object
 * @param c the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_char(pn_data_t *data, pn_char_t c);

/**
 * Puts a ::PN_ULONG value.
 *
 * @param data a pn_data_t object
 * @param ul the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_ulong(pn_data_t *data, uint64_t ul);

/**
 * Puts a ::PN_LONG value.
 *
 * @param data a pn_data_t object
 * @param l the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_long(pn_data_t *data, int64_t l);

/**
 * Puts a ::PN_TIMESTAMP value.
 *
 * @param data a pn_data_t object
 * @param t the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_timestamp(pn_data_t *data, pn_timestamp_t t);

/**
 * Puts a ::PN_FLOAT value.
 *
 * @param data a pn_data_t object
 * @param f the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_float(pn_data_t *data, float f);

/**
 * Puts a ::PN_DOUBLE value.
 *
 * @param data a pn_data_t object
 * @param d the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_double(pn_data_t *data, double d);

/**
 * Puts a ::PN_DECIMAL32 value.
 *
 * @param data a pn_data_t object
 * @param d the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_decimal32(pn_data_t *data, pn_decimal32_t d);

/**
 * Puts a ::PN_DECIMAL64 value.
 *
 * @param data a pn_data_t object
 * @param d the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_decimal64(pn_data_t *data, pn_decimal64_t d);

/**
 * Puts a ::PN_DECIMAL128 value.
 *
 * @param data a pn_data_t object
 * @param d the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_decimal128(pn_data_t *data, pn_decimal128_t d);

/**
 * Puts a ::PN_UUID value.
 *
 * @param data a pn_data_t object
 * @param u the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_uuid(pn_data_t *data, pn_uuid_t u);

/**
 * Puts a ::PN_BINARY value. The bytes referenced by the pn_bytes_t
 * argument are copied and stored inside the pn_data_t object.
 *
 * @param data a pn_data_t object
 * @param bytes the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes);

/**
 * Puts a ::PN_STRING value. The bytes referenced by the pn_bytes_t
 * argument are copied and stored inside the pn_data_t object.
 *
 * @param data a pn_data_t object
 * @param string utf8 encoded unicode
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_string(pn_data_t *data, pn_bytes_t string);

/**
 * Puts a ::PN_SYMBOL value. The bytes referenced by the pn_bytes_t
 * argument are copied and stored inside the pn_data_t object.
 *
 * @param data a pn_data_t object
 * @param symbol ascii encoded symbol
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol);

/**
 * Puts any scalar value value.
 *
 * @param data a pn_data_t object
 * @param atom the value
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_put_atom(pn_data_t *data, pn_atom_t atom);

/**
 * If the current node is a list, return the number of elements,
 * otherwise return zero. List elements can be accessed by entering
 * the list.
 *
 * @code
 *   ...
 *   size_t count = pn_data_get_list(data);
 *   pn_data_enter(data);
 *   for (size_t i = 0; i < count; i++) {
 *       if (pn_data_next(data)) {
 *         switch (pn_data_type(data)) {
 *         case PN_STRING:
 *           ...
 *           break;
 *         case PN_INT:
 *           ...
 *           break;
 *        }
 *   }
 *   pn_data_exit(data);
 *   ...
 * @endcode
.*
 * @param data a pn_data_t object
 * @return the size of a list node
 */
PN_EXTERN size_t pn_data_get_list(pn_data_t *data);

/**
 * If the current node is a map, return the number of child elements,
 * otherwise return zero. Key value pairs can be accessed by entering
 * the map.
 *
 * @code
 *   ...
 *   size_t count = pn_data_get_map(data);
 *   pn_data_enter(data);
 *   for (size_t i = 0; i < count/2; i++) {
 *     // read key
 *     if (pn_data_next(data)) {
 *       switch (pn_data_type(data)) {
 *         case PN_STRING:
 *           ...
 *         break;
 *         ...
 *       }
 *     }
 *     ...
 *     // read value
 *     if (pn_data_next(data)) {
 *       switch (pn_data_type(data)) {
 *         case PN_INT:
 *           ...
 *         break;
 *         ...
 *       }
 *     }
 *     ...
 *   }
 *   pn_data_exit(data);
 *   ...
 * @endcode
 *
 * @param data a pn_data_t object
 * @return the number of child elements of a map node
 */
PN_EXTERN size_t pn_data_get_map(pn_data_t *data);

/**
 * If the current node is an array, return the number of elements in
 * the array, otherwise return 0. Array data can be accessed by
 * entering the array. If the array is described, the first child node
 * will be the descriptor, and the remaining @var count child nodes
 * will be the elements of the array.
 *
 * @code
 *   ...
 *   size_t count = pn_data_get_array(data);
 *   bool described = pn_data_is_array_described(data);
 *   pn_type_t type = pn_data_get_array_type(data);
 *
 *   pn_data_enter(data);
 *
 *   if (described && pn_data_next(data)) {
 *       // the descriptor could be another type, but let's assume it's a symbol
 *       pn_bytes_t descriptor = pn_data_get_symbol(data);
 *   }
 *
 *   for (size_t i = 0; i < count; i++) {
 *     if (pn_data_next(data)) {
 *         // all elements will be values of the array type retrieved above
 *         ...
 *     }
 *   }
 *   pn_data_exit(data);
 *   ...
 * @endcode
 *
 * @param data a pn_data_t object
 * @return the number of elements of an array node
 */
PN_EXTERN size_t pn_data_get_array(pn_data_t *data);

/**
 * Returns true if the current node points to a described array.
 *
 * @param data a pn_data_t object
 * @return true if the current node points to a described array
 */
PN_EXTERN bool pn_data_is_array_described(pn_data_t *data);

/**
 * Return the array type if the current node points to an array,
 * undefined otherwise.
 *
 * @param data a pn_data_t object
 * @return the element type of an array node
 */
PN_EXTERN pn_type_t pn_data_get_array_type(pn_data_t *data);

/**
 * Checks if the current node is a described value. The descriptor and
 * value may be accessed by entering the described value node.
 *
 * @code
 *   ...
 *   // read a symbolically described string
 *   if (pn_data_is_described(data)) {
 *     pn_data_enter(data);
 *     pn_data_next(data);
 *     assert(pn_data_type(data) == PN_SYMBOL);
 *     pn_bytes_t symbol = pn_data_get_symbol(data);
 *     pn_data_next(data);
 *     assert(pn_data_type(data) == PN_STRING);
 *     pn_bytes_t utf8 = pn_data_get_string(data);
 *     pn_data_exit(data);
 *   }
 *   ...
 * @endcode
 *
 * @param data a pn_data_t object
 * @return true if the current node is a described type
 */
PN_EXTERN bool pn_data_is_described(pn_data_t *data);

/**
 * Checks if the current node is a ::PN_NULL.
 *
 * @param data a pn_data_t object
 * @return true iff the current node is ::PN_NULL
 */
PN_EXTERN bool pn_data_is_null(pn_data_t *data);

/**
 * If the current node is a ::PN_BOOL, returns its value.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN bool pn_data_get_bool(pn_data_t *data);

/**
 * If the current node is a ::PN_UBYTE, return its value, otherwise
 * return 0.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN uint8_t pn_data_get_ubyte(pn_data_t *data);

/**
 * If the current node is a signed byte, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN int8_t pn_data_get_byte(pn_data_t *data);

/**
 * If the current node is an unsigned short, returns its value,
 * returns 0 otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN uint16_t pn_data_get_ushort(pn_data_t *data);

/**
 * If the current node is a signed short, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN int16_t pn_data_get_short(pn_data_t *data);

/**
 * If the current node is an unsigned int, returns its value, returns
 * 0 otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN uint32_t pn_data_get_uint(pn_data_t *data);

/**
 * If the current node is a signed int, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN int32_t pn_data_get_int(pn_data_t *data);

/**
 * If the current node is a char, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_char_t pn_data_get_char(pn_data_t *data);

/**
 * If the current node is an unsigned long, returns its value, returns
 * 0 otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN uint64_t pn_data_get_ulong(pn_data_t *data);

/**
 * If the current node is an signed long, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN int64_t pn_data_get_long(pn_data_t *data);

/**
 * If the current node is a timestamp, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_timestamp_t pn_data_get_timestamp(pn_data_t *data);

/**
 * If the current node is a float, returns its value, raises 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN float pn_data_get_float(pn_data_t *data);

/**
 * If the current node is a double, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN double pn_data_get_double(pn_data_t *data);

/**
 * If the current node is a decimal32, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_decimal32_t pn_data_get_decimal32(pn_data_t *data);

/**
 * If the current node is a decimal64, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_decimal64_t pn_data_get_decimal64(pn_data_t *data);

/**
 * If the current node is a decimal128, returns its value, returns 0
 * otherwise.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_decimal128_t pn_data_get_decimal128(pn_data_t *data);

/**
 * If the current node is a UUID, returns its value, returns None
 * otherwise.
 *
 * @param data a pn_data_t object
 * @return a uuid value
 */
PN_EXTERN pn_uuid_t pn_data_get_uuid(pn_data_t *data);

/**
 * If the current node is binary, returns its value, returns ""
 * otherwise. The pn_bytes_t returned will point to memory held inside
 * the pn_data_t. When the pn_data_t is cleared or freed, this memory
 * will be reclaimed.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN pn_bytes_t pn_data_get_binary(pn_data_t *data);

/**
 * If the current node is a string, returns its value, returns ""
 * otherwise. The pn_bytes_t returned will point to memory held inside
 * the pn_data_t. When the pn_data_t is cleared or freed, this memory
 * will be reclaimed.
 *
 * @param data a pn_data_t object
 * @return a pn_bytes_t pointing to utf8
 */
PN_EXTERN pn_bytes_t pn_data_get_string(pn_data_t *data);

/**
 * If the current node is a symbol, returns its value, returns ""
 * otherwise. The pn_bytes_t returned will point to memory held inside
 * the pn_data_t. When the pn_data_t is cleared or freed, this memory
 * will be reclaimed.
 *
 * @param data a pn_data_t object
 * @return a pn_bytes_t pointing to ascii
 */
PN_EXTERN pn_bytes_t pn_data_get_symbol(pn_data_t *data);

/**
 * If the current node is a symbol, string, or binary, return the
 * bytes representing its value. The pn_bytes_t returned will point to
 * memory held inside the pn_data_t. When the pn_data_t is cleared or
 * freed, this memory will be reclaimed.
 *
 * @param data a pn_data_t object
 * @return a pn_bytes_t pointing to the node's value
 */
PN_EXTERN pn_bytes_t pn_data_get_bytes(pn_data_t *data);

/**
 * If the current node is a scalar value, return it as a pn_atom_t.
 *
 * @param data a pn_data_t object
 * @return the value of the current node as pn_atom_t
 */
PN_EXTERN pn_atom_t pn_data_get_atom(pn_data_t *data);

/**
 * Copy the contents of another pn_data_t object. Any values in the
 * data object will be lost.
 *
 * @param data a pn_data_t object
 * @param src the sourc pn_data_t to copy from
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_copy(pn_data_t *data, pn_data_t *src);

/**
 * Append the contents of another pn_data_t object.
 *
 * @param data a pn_data_t object
 * @param src the sourc pn_data_t to append from
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_append(pn_data_t *data, pn_data_t *src);

/**
 * Append up to _n_ values from the contents of another pn_data_t
 * object.
 *
 * @param data a pn_data_t object
 * @param src the sourc pn_data_t to append from
 * @param limit the maximum number of values to append
 * @return zero on success or an error code on failure
 */
PN_EXTERN int pn_data_appendn(pn_data_t *data, pn_data_t *src, int limit);

/**
 * Modify a pn_data_t object to behave as if the current node is the
 * root node of the tree. This impacts the behaviour of
 * ::pn_data_rewind(), ::pn_data_next(), ::pn_data_prev(), and
 * anything else that depends on the navigational state of the
 * pn_data_t object. Use ::pn_data_widen() to reverse the effect of
 * this operation.
 *
 * @param data a pn_data_t object
 */
PN_EXTERN void pn_data_narrow(pn_data_t *data);

/**
 * Reverse the effect of ::pn_data_narrow().
 *
 * @param data a pn_data_t object
 */
PN_EXTERN void pn_data_widen(pn_data_t *data);

/**
 * Returns a handle for the current navigational state of a pn_data_t
 * so that it can be later restored using ::pn_data_restore().
 *
 * @param data a pn_data_t object
 * @return a handle for the current navigational state
 */
PN_EXTERN pn_handle_t pn_data_point(pn_data_t *data);

/**
 * Restores a prior navigational state that was saved using
 * ::pn_data_point(). If the data object has been modified in such a
 * way that the prior navigational state cannot be restored, then this
 * will return false and the navigational state will remain unchanged,
 * otherwise it will return true.
 *
 * @param data a pn_data_t object
 * @param handle a handle referencing the saved navigational state
 * @return true iff the prior navigational state was restored
 */
PN_EXTERN bool pn_data_restore(pn_data_t *data, pn_handle_t point);

/**
 * Dumps a debug representation of the internal state of the pn_data_t
 * object that includes its navigational state to stdout for debugging
 * purposes.
 *
 * @param data a pn_data_t object that is behaving in a confusing way
 */
PN_EXTERN void pn_data_dump(pn_data_t *data);

/** @}
 */

#ifdef __cplusplus
}
#endif

#endif /* codec.h */
