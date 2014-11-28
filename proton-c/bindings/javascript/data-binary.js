/*
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

/*****************************************************************************/
/*                                                                           */
/*                             proton.Data.Binary                            */
/*                                                                           */
/*****************************************************************************/

/**
 * Create a proton.Data.Binary. This constructor takes one or two parameters.
 * The first parameter may serve different purposes depending on its type;
 * If value is a number then it represents the size of the Binary data buffer,
 * if it is a string then we copy the string to the buffer, if it is an Array
 * or a TypedArray then we copy the data to the buffer. The optional second
 * parameter is a pointer to the data in an internal data store. If start is
 * not specified then the number of bytes specified in the first parameter
 * will be allocated in the internal data store and start will point to the
 * start of that block of data.
 * @classdesc
 * This class represents an AMQP Binary type. This class allows us to create and
 * use raw binary data and map it efficiently between JavaScript client code and
 * the underlying implementation, where all data is managed on a "virtual heap".
 * <p>
 * Client applications should generally not have to care about memory management
 * as, for most common use cases, client applications would "transfer ownership"
 * to a "container" which would then "own" the underlying data and free the data
 * held by the {@link proton.Data.Binary}.
 * <p>
 * As an example one common use-case would be where client application creates a
 * {@link proton.Data.Binary} specifying the required size. It would usually then
 * call getBuffer() to access the underlying Uint8Array. At this point the client
 * "owns" the data and so would have to call free() if it did nothing more with
 * the Binary, however when {@link proton.Data.putBINARY} is called the ownership
 * of the raw data on the virtual heap transfers from the Binary to the Data and
 * the client no longer needs to call free() itself. In this case the putBINARY()
 * call transfers ownership and can then safely call free() on the Binary.
 * <p>
 * Conversely a common use-case when receiving data is where a Binary may be
 * created by {@link proton.Data#getBINARY}. In this case the Binary is simply a
 * "view" onto the bytes owned by the Data instance. A client application can
 * safely access the bytes from the view, but if it wishes to use the bytes beyond
 * the scope of the Data instance (e.g. after the next {@link proton.Messenger#get}
 * call then the client must explicitly *copy* the bytes into a new buffer, for
 * example via copyBuffer().
 * <p>
 * Most of the {@link proton.Data} methods that take {@link proton.Data.Binary}
 * as a parameter "consume" the underlying data and take responsibility for
 * freeing it from the heap e.g. {@link proton.Data#putBINARY}, {@link proton.Data#decode}.
 * For the methods that return a {@link proton.Data.Binary} the call to
 * {@link proton.Data#getBINARY}, which is the most common case, returns a Binary
 * that has a "view" of the underlying data that is actually owned by the Data
 * instance and thus doesn't need to be explicitly freed on the Binary. The call
 * to {@link proton.Data#encode} however returns a Binary that represents a *copy*
 * of the underlying data, in this case (like a client calling new proton.Data.Binary)
 * the client takes responsibility for freeing the data, unless of course it is
 * subsequently passed to a method that will consume the data (putBINARY/decode).
 * @constructor proton.Data.Binary
 * @param {(number|string|Array|TypedArray)} value If value is a number then it 
 *        represents the size of the Binary data buffer, if it is a string then
 *        we copy the string to the buffer, if it is an Array or a TypedArray
 *        then we copy the data to the buffer. N.B. although convenient do bear
 *        in mind that using a mechanism other than constructing with a simple
 *        size will result in some form of additional data copy.
 * @param {number} start an optional pointer to the start of the Binary data buffer.
 */
Data['Binary'] = function(value, start) { // Data.Binary Constructor.
    /**
     * If the start pointer is specified then the underlying binary data is owned
     * by another object, so we set the call to free to be a null function. If
     * the start pointer is not passed then we allocate storage of the specified
     * size on the emscripten heap and set the call to free to free the data from
     * the emscripten heap.
     */
    var size = value;
    if (start) {
        this['free'] = function() {};
    } else { // Create Binary from Array, ArrayBuffer or TypedArray.
        var hasArrayBuffer = (typeof ArrayBuffer === 'function');
        if (Data.isArray(value) ||
            (hasArrayBuffer && value instanceof ArrayBuffer) || 
            (value.buffer && hasArrayBuffer && value.buffer instanceof ArrayBuffer)) {
            value = new Uint8Array(value);
            size = value.length;
            start = _malloc(size); // Allocate storage from emscripten heap.
            Module['HEAPU8'].set(value, start); // Copy the data to the emscripten heap.
        } else if (Data.isString(value)) { // Create Binary from native string
            value = unescape(encodeURIComponent(value)); // Create a C-like UTF representation.
            size = value.length;
            start = _malloc(size); // Allocate storage from emscripten heap.
            for (var i = 0; i < size; i++) {
                setValue(start + i, value.charCodeAt(i), 'i8', 1);
            }
        } else { // Create unpopulated Binary of specified size.
            // If the type is not a number by this point then an unrecognised data
            // type has been passed so we create a zero length Binary.
            size = Data.isNumber(size) ? size : 0;
            start = _malloc(size); // Allocate storage from emscripten heap.
        }
        this['free'] = function() {
            _free(this.start);
            this.size = 0;
            this.start = 0;
            // Set free to a null function to prevent possibility of a "double free".
            this['free'] = function() {};
        };
    }

    this.size = size;
    this.start = start;
};

/**
 * Get a Uint8Array view of the data. N.B. this is just a *view* of the data,
 * which will go out of scope on the next call to {@link proton.Messenger.get}. If
 * a client wants to retain the data then copy should be used to explicitly
 * create a copy of the data which the client then owns to do with as it wishes.
 * @method getBuffer
 * @returns {Uint8Array} a new Uint8Array view of the data.
 * @memberof! proton.Data.Binary#
 */
Data['Binary'].prototype['getBuffer'] = function() {
    return new Uint8Array(HEAPU8.buffer, this.start, this.size);
};

/**
 * Explicitly create a *copy* of the Binary, copying the underlying binary data too.
 * @method copy
 * @param {number} offset an optional offset into the underlying buffer from
 *        where we want to copy the data, default is zero.
 * @param {number} n an optional number of bytes to copy, default is this.size - offset.
 * @returns {proton.Data.Binary} a new {@link proton.Data.Binary} created by copying the underlying binary data.
 * @memberof! proton.Data.Binary#
 */
Data['Binary'].prototype['copy'] = function(offset, n) {
    offset = offset | 0;
    n = n ? n : (this.size - offset);

    if (offset >= this.size) {
        offset = 0;
        n = 0;
    } else if ((offset + n) > this.size) {
        n = this.size - offset; // Clamp length
    }

    var start = _malloc(n); // Allocate storage from emscripten heap.
    _memcpy(start, this.start + offset, n); // Copy the raw data to new buffer.

    return new Data['Binary'](n, start);
};

/**
 * Converts the {@link proton.Data.Binary} to a string. This is clearly most
 * useful when the binary data is actually a binary representation of a string
 * such as a C style ASCII string.
 * @method toString
 * @memberof! proton.Data.Binary#
 * @returns {string} the String form of a {@link proton.Data.Binary}.
 */
Data['Binary'].prototype.toString = Data['Binary'].prototype.valueOf = function() {
    // Create a native JavaScript String from the start/size information.
    return Pointer_stringify(this.start, this.size);
};

