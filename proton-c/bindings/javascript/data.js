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
/*                                    Data                                   */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.Data instance.
 * @classdesc
 * The Data class provides an interface for decoding, extracting, creating, and
 * encoding arbitrary AMQP data. A Data object contains a tree of AMQP values.
 * Leaf nodes in this tree correspond to scalars in the AMQP type system such as
 * ints<INT> or strings<STRING>. Non-leaf nodes in this tree correspond to compound
 * values in the AMQP type system such as lists<LIST>, maps<MAP>, arrays<ARRAY>,
 * or described values<DESCRIBED>. The root node of the tree is the Data object
 * itself and can have an arbitrary number of children.
 * <p>
 * A Data object maintains the notion of the current sibling node and a current
 * parent node. Siblings are ordered within their parent. Values are accessed
 * and/or added by using the next, prev, enter, and exit methods to navigate to
 * the desired location in the tree and using the supplied variety of put* and
 * get* methods to access or add a value of the desired type.
 * <p>
 * The put* methods will always add a value after the current node in the tree.
 * If the current node has a next sibling the put* method will overwrite the value
 * on this node. If there is no current node or the current node has no next
 * sibling then one will be added. The put* methods always set the added/modified
 * node to the current node. The get* methods read the value of the current node
 * and do not change which node is current.
 * @constructor proton.Data
 * @param {number} data an optional pointer to a pn_data_t instance. If supplied
 *        the underlying data is "owned" by another object (for example a Message)
 *        and that object is assumed to be responsible for freeing the data if
 *        necessary. If no data is supplied then the Data is stand-alone and the
 *        client application is responsible for freeing the underlying data via
 *        a call to free().
 * @param {boolean} decodeBinaryAsString if set decode any AMQP Binary payload
 *        objects as strings. This can be useful as the data in Binary objects
 *        will be overwritten with subsequent calls to get, so they must be
 *        explicitly copied. Needless to say it is only safe to set this flag if
 *        you know that the data you are dealing with is actually a string, for
 *        example C/C++ applications often seem to encode strings as AMQP binary,
 *        a common cause of interoperability problems.
 */
Module['Data'] = function(data, decodeBinaryAsString) { // Data Constructor.
    if (!data) {
        this._data = _pn_data(16); // Default capacity is 16
        this['free'] = function() {
            _pn_data_free(this._data);
            // Set free to a null function to prevent possibility of a "double free".
            this['free'] = function() {};
        };
    } else {
        this._data = data;
        this['free'] = function() {};
    }
    this._decodeBinaryAsString = decodeBinaryAsString;
};

// Expose constructor as package scope variable to make internal calls less verbose.
var Data = Module['Data'];

// Expose prototype as a variable to make method declarations less verbose.
var _Data_ = Data.prototype;

// ************************** Class properties ********************************

Data['NULL']       = 1;
Data['BOOL']       = 2;
Data['UBYTE']      = 3;
Data['BYTE']       = 4;
Data['USHORT']     = 5;
Data['SHORT']      = 6;
Data['UINT']       = 7;
Data['INT']        = 8;
Data['CHAR']       = 9;
Data['ULONG']      = 10;
Data['LONG']       = 11;
Data['TIMESTAMP']  = 12;
Data['FLOAT']      = 13;
Data['DOUBLE']     = 14;
Data['DECIMAL32']  = 15;
Data['DECIMAL64']  = 16;
Data['DECIMAL128'] = 17;
Data['UUID']       = 18;
Data['BINARY']     = 19;
Data['STRING']     = 20;
Data['SYMBOL']     = 21;
Data['DESCRIBED']  = 22;
Data['ARRAY']      = 23;
Data['LIST']       = 24;
Data['MAP']        = 25;

/**
 * Look-up table mapping proton-c types to the accessor method used to
 * deserialise the type. N.B. this is a simple Array and not a map because the
 * types that we get back from pn_data_type are integers from the pn_type_t enum.
 * @property {Array<String>} TypeNames ['NULL', 'NULL', 'BOOL', 'UBYTE', 'BYTE',
 * 'USHORT', 'SHORT', 'UINT', 'INT', 'CHAR', 'ULONG', 'LONG', 'TIMESTAMP',
 * 'FLOAT', 'DOUBLE', 'DECIMAL32', 'DECIMAL64', 'DECIMAL128', 'UUID',
 * 'BINARY', 'STRING', 'SYMBOL', 'DESCRIBED', 'ARRAY', 'LIST', 'MAP']
 * @memberof! proton.Data
 */
Data['TypeNames'] = [
    'NULL',       // 0
    'NULL',       // PN_NULL       = 1
    'BOOL',       // PN_BOOL       = 2
    'UBYTE',      // PN_UBYTE      = 3
    'BYTE',       // PN_BYTE       = 4
    'USHORT',     // PN_USHORT     = 5
    'SHORT',      // PN_SHORT      = 6
    'UINT',       // PN_UINT       = 7
    'INT',        // PN_INT        = 8
    'CHAR',       // PN_CHAR       = 9
    'ULONG',      // PN_ULONG      = 10
    'LONG',       // PN_LONG       = 11
    'TIMESTAMP',  // PN_TIMESTAMP  = 12
    'FLOAT',      // PN_FLOAT      = 13
    'DOUBLE',     // PN_DOUBLE     = 14
    'DECIMAL32',  // PN_DECIMAL32  = 15
    'DECIMAL64',  // PN_DECIMAL64  = 16
    'DECIMAL128', // PN_DECIMAL128 = 17
    'UUID',       // PN_UUID       = 18
    'BINARY',     // PN_BINARY     = 19
    'STRING',     // PN_STRING     = 20
    'SYMBOL',     // PN_SYMBOL     = 21
    'DESCRIBED',  // PN_DESCRIBED  = 22
    'ARRAY',      // PN_ARRAY      = 23
    'LIST',       // PN_LIST       = 24
    'MAP'         // PN_MAP        = 25
];

// *************************** Class methods **********************************

/**
 * Test if a given Object is a JavaScript Array.
 * @method isArray
 * @memberof! proton.Data
 * @param {object} o the Object that we wish to test.
 * @returns {boolean} true iff the Object is a JavaScript Array.
 */
Data.isArray = Array.isArray || function(o) {
    return Object.prototype.toString.call(o) === '[object Array]';
};

/**
 * Test if a given Object is a JavaScript Number.
 * @method isNumber
 * @memberof! proton.Data
 * @param {object} o the Object that we wish to test.
 * @returns {boolean} true iff the Object is a JavaScript Number.
 */
Data.isNumber = function(o) {
    return typeof o === 'number' || 
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object Number]');
};

/**
 * Test if a given Object is a JavaScript String.
 * @method isString
 * @memberof! proton.Data
 * @param {object} o the Object that we wish to test.
 * @returns {boolean} true iff the Object is a JavaScript String.
 */
Data.isString = function(o) {
    return typeof o === 'string' ||
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object String]');
};

/**
 * Test if a given Object is a JavaScript Boolean.
 * @method isBoolean
 * @memberof! proton.Data
 * @param {object} o the Object that we wish to test.
 * @returns {boolean} true iff the Object is a JavaScript Boolean.
 */
Data.isBoolean = function(o) {
    return typeof o === 'boolean' ||
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object Boolean]');
};


// ************************* Protected methods ********************************

// We use the dot notation rather than associative array form for protected
// methods so they are visible to this "package", but the Closure compiler will
// minify and obfuscate names, effectively making a defacto "protected" method.

/**
 * This helper method checks the supplied error code, converts it into an
 * exception and throws the exception. This method will try to use the message
 * populated in pn_data_error(), if present, but if not it will fall
 * back to using the basic error code rendering from pn_code().
 * @param code the error code to check.
 */
_Data_._check = function(code) {
    if (code < 0) {
        var errno = this['getErrno']();
        var message = errno ? this['getError']() : Pointer_stringify(_pn_code(code));

        throw new Module['DataError'](message);
    } else {
        return code;
    }
};


// *************************** Public methods *********************************

/**
 * @method getErrno
 * @memberof! proton.Data#
 * @returns {number} the most recent error message code.
 */
_Data_['getErrno'] = function() {
    return _pn_data_errno(this._data);
};

/**
 * @method getError
 * @memberof! proton.Data#
 * @returns {string} the most recent error message as a String.
 */
_Data_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_data_error(this._data)));
};

/**
 * Clears the data object.
 * @method clear
 * @memberof! proton.Data#
 */
_Data_['clear'] = function() {
    _pn_data_clear(this._data);
};

/**
 * Clears current node and sets the parent to the root node.  Clearing the current
 * node sets it _before_ the first node, calling next() will advance to the first node.
 * @method rewind
 * @memberof! proton.Data#
 */
_Data_['rewind'] = function() {
    _pn_data_rewind(this._data);
};

/**
 * Advances the current node to its next sibling and returns its type. If there
 * is no next sibling the current node remains unchanged and null is returned.
 * @method next
 * @memberof! proton.Data#
 * @returns {number} the type of the next sibling or null.
 */
_Data_['next'] = function() {
    var found = _pn_data_next(this._data);
    if (found) {
        return this.type();
    } else {
        return null;
    }
};

/**
 * Advances the current node to its previous sibling and returns its type. If there
 * is no previous sibling the current node remains unchanged and null is returned.
 * @method prev
 * @memberof! proton.Data#
 * @returns {number} the type of the previous sibling or null.
 */
_Data_['prev'] = function() {
    var found = _pn_data_prev(this._data);
    if (found) {
        return this.type();
    } else {
        return null;  
    }
};

/**
 * Sets the parent node to the current node and clears the current node. Clearing
 * the current node sets it _before_ the first child, next() advances to the first child.
 * @method enter
 * @memberof! proton.Data#
 */
_Data_['enter'] = function() {
    return (_pn_data_enter(this._data) > 0);
};

/**
 * Sets the current node to the parent node and the parent node to its own parent.
 * @method exit
 * @memberof! proton.Data#
 */
_Data_['exit'] = function() {
    return (_pn_data_exit(this._data) > 0);
};

/**
 * Look up a value by name. N.B. Need to use getObject() to retrieve the actual
 * value after lookup suceeds.
 * @method lookup
 * @memberof! proton.Data#
 * @param {string} name the name of the property to look up.
 * @returns {boolean} true iff the lookup succeeded.
 */
_Data_['lookup'] = function(name) {
    var sp = Runtime.stackSave();
    var lookup = _pn_data_lookup(this._data, allocate(intArrayFromString(name), 'i8', ALLOC_STACK));
    Runtime.stackRestore(sp);
    return (lookup > 0);
};

// TODO document - not quite sure what these are for?
_Data_['narrow'] = function() {
    _pn_data_narrow(this._data);
};

_Data_['widen'] = function() {
    _pn_data_widen(this._data);
};

/**
 * @method type
 * @memberof! proton.Data#
 * @returns {number} the type of the current node or null if the type is unknown.
 */
_Data_['type'] = function() {
    var dtype = _pn_data_type(this._data);
    if (dtype === -1) {
        return null;
    } else {
        return dtype;
    }
};

/**
 * Return a Binary representation of the data encoded in AMQP format. N.B. the
 * returned {@link proton.Data.Binary} "owns" the underlying raw data and is thus
 * responsible for freeing it or passing it to a method that consumes a Binary
 * such as {@link proton.Data.decode} or {@link proton.Data.putBINARY}.
 * @method encode
 * @memberof! proton.Data#
 * @returns {proton.Data.Binary} a representation of the data encoded in AMQP format.
 */
_Data_['encode'] = function() {
    var size = 1024;
    while (true) {
        var bytes = _malloc(size); // Allocate storage from emscripten heap.
        var cd = _pn_data_encode(this._data, bytes, size);

        if (cd === Module['Error']['OVERFLOW']) {
            _free(bytes);
            size *= 2;
        } else if (cd >= 0) {
            return new Data['Binary'](cd, bytes);
        } else {
            _free(bytes);
            this._check(cd);
            return;
        }
    }
};

/**
 * Decodes the first value from supplied Binary AMQP data and returns a new
 * {@link proton.Data.Binary} containing the remainder of the data or null if
 * all the supplied data has been consumed. N.B. this method "consumes" data
 * from a {@link proton.Data.Binary} in other words it takes responsibility for
 * the underlying data and frees the raw data from the Binary.
 * @method decode
 * @memberof! proton.Data#
 * @param {proton.Data.Binary} encoded the AMQP encoded binary data.
 * @returns {proton.Data.Binary} a Binary containing the remaining bytes or null
 *          if all the data has been consumed.
 */
_Data_['decode'] = function(encoded) {
    var start = encoded.start;
    var size = encoded.size;
    var consumed = this._check(_pn_data_decode(this._data, start, size));

    size = size - consumed;
    start = _malloc(size); // Allocate storage from emscripten heap.
    _memcpy(start, encoded.start + consumed, size);

    encoded['free'](); // Free the original Binary.
    return size > 0 ? new Data['Binary'](size, start) : null;
};

/**
 * Puts a list node. Elements may be filled by entering the list
 * node and putting element values.
 * <pre>
 *  var data = new proton.Data();
 *  data.putLISTNODE();
 *  data.enter();
 *  data.putINT(1);
 *  data.putINT(2);
 *  data.putINT(3);
 *  data.exit();
 * </pre>
 * @method putLISTNODE
 * @memberof! proton.Data#
 */
_Data_['putLISTNODE'] = function() {
    this._check(_pn_data_put_list(this._data));
};

/**
 * Puts a map node. Elements may be filled by entering the map node
 * and putting alternating key value pairs.
 * <pre>
 *  var data = new proton.Data();
 *  data.putMAPNODE();
 *  data.enter();
 *  data.putSTRING('key');
 *  data.putSTRING('value');
 *  data.exit();
 * </pre>
 * @method putMAPNODE
 * @memberof! proton.Data#
 */
_Data_['putMAPNODE'] = function() {
    this._check(_pn_data_put_map(this._data));
};

/**
 * Puts an array node. Elements may be filled by entering the array node and
 * putting the element values. The values must all be of the specified array
 * element type. If an array is described then the first child value of the array
 * is the descriptor and may be of any type.
 * <pre>
 *  var data = new proton.Data();
 *  data.putARRAYNODE(false, proton.Data.INT);
 *  data.enter();
 *  data.putINT(1);
 *  data.putINT(2);
 *  data.putINT(3);
 *  data.exit();
 *
 *  data.putARRAYNODE(true, proton.Data.DOUBLE);
 *  data.enter();
 *  data.putSYMBOL('array-descriptor');
 *  data.putDOUBLE(1.1);
 *  data.putDOUBLE(1.2);
 *  data.putDOUBLE(1.3);
 *  data.exit();
 * </pre>
 * @method putARRAYNODE
 * @param {boolean} described specifies whether the array is described.
 * @param {number} type the type of the array elements.
 * @memberof! proton.Data#
 */
_Data_['putARRAYNODE'] = function(described, type) {
    this._check(_pn_data_put_array(this._data, described, type));
};

/**
 * Puts a described node. A described node has two children, the descriptor and
 * value. These are specified by entering the node and putting the desired values.
 * <pre>
 *  var data = new proton.Data();
 *  data.putDESCRIBEDNODE();
 *  data.enter();
 *  data.putSYMBOL('value-descriptor');
 *  data.putSTRING('the value');
 *  data.exit();
 * </pre>
 * @method putDESCRIBEDNODE
 * @memberof! proton.Data#
 */
_Data_['putDESCRIBEDNODE'] = function() {
    this._check(_pn_data_put_described(this._data));
};

/**
 * Puts a null value.
 * @method putNULL
 * @memberof! proton.Data#
 */
_Data_['putNULL'] = function() {
    this._check(_pn_data_put_null(this._data));
};

/**
 * Puts a boolean value.
 * @method putBOOL
 * @memberof! proton.Data#
 * @param {boolean} b a boolean value.
 */
_Data_['putBOOL'] = function(b) {
    this._check(_pn_data_put_bool(this._data, b));
};

/**
 * Puts a unsigned byte value.
 * @method putUBYTE
 * @memberof! proton.Data#
 * @param {number} ub an integral value.
 */
_Data_['putUBYTE'] = function(ub) {
    this._check(_pn_data_put_ubyte(this._data, ub));
};

/**
 * Puts a signed byte value.
 * @method putBYTE
 * @memberof! proton.Data#
 * @param {number} b an integral value.
 */
_Data_['putBYTE'] = function(b) {
    this._check(_pn_data_put_byte(this._data, b));
};

/**
 * Puts a unsigned short value.
 * @method putUSHORT
 * @memberof! proton.Data#
 * @param {number} us an integral value.
 */
_Data_['putUSHORT'] = function(us) {
    this._check(_pn_data_put_ushort(this._data, us));
};

/**
 * Puts a signed short value.
 * @method putSHORT
 * @memberof! proton.Data#
 * @param {number} s an integral value.
 */
_Data_['putSHORT'] = function(s) {
    this._check(_pn_data_put_short(this._data, s));
};

/**
 * Puts a unsigned integer value.
 * @method putUINT
 * @memberof! proton.Data#
 * @param {number} ui an integral value.
 */
_Data_['putUINT'] = function(ui) {
    this._check(_pn_data_put_uint(this._data, ui));
};

/**
 * Puts a signed integer value.
 * @method putINT
 * @memberof! proton.Data#
 * @param {number} i an integral value.
 */
_Data_['putINT'] = function(i) {
    this._check(_pn_data_put_int(this._data, i));
};

/**
 * Puts a signed char value.
 * @method putCHAR
 * @memberof! proton.Data#
 * @param {(string|number)} c a single character expressed either as a string or a number.
 */
_Data_['putCHAR'] = function(c) {
    c = Data.isString(c) ? c.charCodeAt(0) : c;
    this._check(_pn_data_put_char(this._data, c));
};

/**
 * Puts a unsigned long value. N.B. large values can suffer from a loss of
 * precision as JavaScript numbers are restricted to 64 bit double values.
 * @method putULONG
 * @memberof! proton.Data#
 * @param {number} ul an integral value.
 */
_Data_['putULONG'] = function(ul) {
    // If the supplied number exceeds the range of Data.Long invert it before
    // constructing the Data.Long.
    ul = (ul >= Data.Long.TWO_PWR_63_DBL_) ? (ul = -(Data.Long.TWO_PWR_64_DBL_ - ul)) : ul;
    var long = Data.Long.fromNumber(ul);
    this._check(_pn_data_put_ulong(this._data, long.getLowBitsUnsigned(), long.getHighBits()));
};

/**
 * Puts a signed long value. N.B. large values can suffer from a loss of
 * precision as JavaScript numbers are restricted to 64 bit double values.
 * @method putLONG
 * @memberof! proton.Data#
 * @param {number} i an integral value.
 */
_Data_['putLONG'] = function(l) {
    var long = Data.Long.fromNumber(l);
    this._check(_pn_data_put_long(this._data, long.getLowBitsUnsigned(), long.getHighBits()));
};

/**
 * Puts a timestamp.
 * @method putTIMESTAMP
 * @memberof! proton.Data#
 * @param {(number|Date)} d a Date value.
 */
_Data_['putTIMESTAMP'] = function(d) {
    // Note that a timestamp is a 64 bit number so we have to use a proton.Data.Long.
    var timestamp = Data.Long.fromNumber(d.valueOf());
    this._check(_pn_data_put_timestamp(this._data, timestamp.getLowBitsUnsigned(), timestamp.getHighBits()));
};

/**
 * Puts a float value. N.B. converting between floats and doubles is imprecise
 * so the resulting value might not quite be what you expect.
 * @method putFLOAT
 * @memberof! proton.Data#
 * @param {number} f a floating point value.
 */
_Data_['putFLOAT'] = function(f) {
    this._check(_pn_data_put_float(this._data, f));
};

/**
 * Puts a double value.
 * @method putDOUBLE
 * @memberof! proton.Data#
 * @param {number} d a floating point value.
 */
_Data_['putDOUBLE'] = function(d) {
    this._check(_pn_data_put_double(this._data, d));
};

/**
 * Puts a decimal32 value.
 * @method putDECIMAL32
 * @memberof! proton.Data#
 * @param {number} d a decimal32 value.
 */
_Data_['putDECIMAL32'] = function(d) {
    this._check(_pn_data_put_decimal32(this._data, d));
};

/**
 * Puts a decimal64 value.
 * @method putDECIMAL64
 * @memberof! proton.Data#
 * @param {number} d a decimal64 value.
 */
_Data_['putDECIMAL64'] = function(d) {
    this._check(_pn_data_put_decimal64(this._data, d));
};

/**
 * Puts a decimal128 value.
 * @method putDECIMAL128
 * @memberof! proton.Data#
 * @param {number} d a decimal128 value.
 */
_Data_['putDECIMAL128'] = function(d) {
    this._check(_pn_data_put_decimal128(this._data, d));
};

/**
 * Puts a UUID value.
 * @method putUUID
 * @memberof! proton.Data#
 * @param {proton.Data.Uuid} u a uuid value
 */
_Data_['putUUID'] = function(u) {
    var sp = Runtime.stackSave();
    this._check(_pn_data_put_uuid(this._data, allocate(u['uuid'], 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Puts a binary value consuming the underlying raw data in the process.
 * @method putBINARY
 * @memberof! proton.Data#
 * @param {proton.Data.Binary} b a binary value.
 */
_Data_['putBINARY'] = function(b) {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_data_put_binary(data, pn_bytes(b.size, b.start));

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the pn_bytes.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, b.size, b.start);

    // The compiled pn_data_put_binary takes the pn_bytes_t by reference not value.
    this._check(_pn_data_put_binary(this._data, bytes));

    // After calling _pn_data_put_binary the underlying Data object "owns" the
    // binary data, so we can call free on the proton.Data.Binary instance to
    // release any storage it has acquired back to the emscripten heap.
    b['free']();
    Runtime.stackRestore(sp);
};

/**
 * Puts a unicode string value.
 * @method putSTRING
 * @memberof! proton.Data#
 * @param {string} s a unicode string value.
 */
_Data_['putSTRING'] = function(s) {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_data_put_string(data, pn_bytes(strlen(text), text));

    // First create an array from the JavaScript String using the intArrayFromString
    // helper function (from emscripten/src/preamble.js). We use this idiom in a
    // few places but here we create array as a separate var as we need its length.
    var array = intArrayFromString(s, true); // The true means don't add NULL.
    // Allocate temporary storage for the array on the emscripten stack.
    var str = allocate(array, 'i8', ALLOC_STACK);

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the pn_bytes.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, array.length, str);

    // The compiled pn_data_put_string takes the pn_bytes_t by reference not value.
    this._check(_pn_data_put_string(this._data, bytes));
    Runtime.stackRestore(sp);
};

/**
 * Puts a symbolic value. According to the AMQP 1.0 Specification Symbols are
 * values from a constrained domain. Although the set of possible domains is
 * open-ended, typically the both number and size of symbols in use for any
 * given application will be small, e.g. small enough that it is reasonable to
 * cache all the distinct values. Symbols are encoded as ASCII characters.
 * @method putSYMBOL
 * @memberof! proton.Data#
 * @param {proton.Data.Symbol|string} s the symbol name.
 */
_Data_['putSYMBOL'] = function(s) {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_data_put_symbol(data, pn_bytes(strlen(text), text));

    // First create an array from the JavaScript String using the intArrayFromString
    // helper function (from emscripten/src/preamble.js). We use this idiom in a
    // few places but here we create array as a separate var as we need its length.
    var array = intArrayFromString(s, true); // The true means don't add NULL.
    // Allocate temporary storage for the array on the emscripten stack.
    var str = allocate(array, 'i8', ALLOC_STACK);

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the pn_bytes.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, array.length, str);

    // The compiled pn_data_put_symbol takes the pn_bytes_t by reference not value.
    this._check(_pn_data_put_symbol(this._data, bytes));
    Runtime.stackRestore(sp);
};

/**
 * If the current node is a list node, return the number of elements,
 * otherwise return zero. List elements can be accessed by entering
 * the list.
 * <pre>
 *  var count = data.getLISTNODE();
 *  data.enter();
 *  for (var i = 0; i < count; i++) {
 *      var type = data.next();
 *      if (type === proton.Data.STRING) {
 *          console.log(data.getSTRING());
 *      }
 *  }
 *  data.exit();
 * </pre>
 * @method getLISTNODE
 * @memberof! proton.Data#
 * @returns {number} the number of elements if the current node is a list,
 *          zero otherwise.
 */
_Data_['getLISTNODE'] = function() {
    return _pn_data_get_list(this._data);
};

/**
 * If the current node is a map, return the number of child elements,
 * otherwise return zero. Key value pairs can be accessed by entering
 * the map.
 * <pre>
 *  var count = data.getMAPNODE();
 *  data.enter();
 *  for (var i = 0; i < count/2; i++) {
 *      var type = data.next();
 *      if (type === proton.Data.STRING) {
 *          console.log(data.getSTRING());
 *      }
 *  }
 *  data.exit();
 * </pre>
 * @method getMAPNODE
 * @memberof! proton.Data#
 * @returns {number} the number of elements if the current node is a list,
 *          zero otherwise.
 */
_Data_['getMAPNODE'] = function() {
    return _pn_data_get_map(this._data);
};

/**
 * If the current node is an array, return an object containing the tuple of the
 * element count, a boolean indicating whether the array is described, and the
 * type of each element, otherwise return {count: 0, described: false, type: null).
 * Array data can be accessed by entering the array.
 * <pre>
 *  // Read an array of strings with a symbolic descriptor
 *  var metadata = data.getARRAYNODE();
 *  var count = metadata.count;
 *  data.enter();
 *  data.next();
 *  console.log("Descriptor:" + data.getSYMBOL());
 *  for (var i = 0; i < count; i++) {
 *      var type = data.next();
 *      console.log("Element:" + data.getSTRING());
 *  }
 *  data.exit();
 * </pre>
 * @method getARRAYNODE
 * @memberof! proton.Data#
 * @returns {object} the tuple of the element count, a boolean indicating whether
 *          the array is described, and the type of each element.
 */
_Data_['getARRAYNODE'] = function() {
    var count = _pn_data_get_array(this._data);
    var described = (_pn_data_is_array_described(this._data) > 0);
    var type = _pn_data_get_array_type(this._data);
    type = (type == -1) ? null : type;
    return {'count': count, 'described': described, 'type': type};
};

/**
 * Checks if the current node is a described node. The descriptor and value may
 * be accessed by entering the described node.
 * <pre>
 *  // read a symbolically described string
 *  assert(data.isDESCRIBEDNODE()); // will error if the current node is not described
 *  data.enter();
 *  console.log(data.getSYMBOL());
 *  console.log(data.getSTRING());
 *  data.exit();
 * </pre>
 * @method isDESCRIBEDNODE
 * @memberof! proton.Data#
 * @returns {boolean} true iff the current node is a described, false otherwise.
 */
_Data_['isDESCRIBEDNODE'] = function() {
    return _pn_data_is_described(this._data);
};

/**
 * @method getNULL
 * @memberof! proton.Data#
 * @returns a null value.
 */
_Data_['getNULL'] = function() {
    return null;
};

/**
 * Checks if the current node is a null.
 * @method isNULL
 * @memberof! proton.Data#
 * @returns {boolean} true iff the current node is null.
 */
_Data_['isNULL'] = function() {
    return (_pn_data_is_null(this._data) > 0);
};

/**
 * @method getBOOL
 * @memberof! proton.Data#
 * @returns {boolean} a boolean value if the current node is a boolean, returns
 *          false otherwise.
 */
_Data_['getBOOL'] = function() {
    return (_pn_data_get_bool(this._data) > 0);
};

/**
 * @method getUBYTE
 * @memberof! proton.Data#
 * @returns {number} value if the current node is an unsigned byte, returns 0 otherwise.
 */
_Data_['getUBYTE'] = function() {
    return _pn_data_get_ubyte(this._data) & 0xFF; // & 0xFF converts to unsigned;
};

/**
 * @method getBYTE
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a signed byte, returns 0 otherwise.
 */
_Data_['getBYTE'] = function() {
    return _pn_data_get_byte(this._data);
};

/**
 * @method getUSHORT
 * @memberof! proton.Data#
 * @returns {number} value if the current node is an unsigned short, returns 0 otherwise.
 */
_Data_['getUSHORT'] = function() {
    return _pn_data_get_ushort(this._data) & 0xFFFF; // & 0xFFFF converts to unsigned;
};

/**
 * @method getSHORT
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a signed short, returns 0 otherwise.
 */
_Data_['getSHORT'] = function() {
    return _pn_data_get_short(this._data);
};

/**
 * @method getUINT
 * @memberof! proton.Data#
 * @returns {number} value if the current node is an unsigned int, returns 0 otherwise.
 */
_Data_['getUINT'] = function() {
    var value = _pn_data_get_uint(this._data);
    return (value > 0) ? value : 4294967296 + value; // 4294967296 == 2^32
};

/**
 * @method getINT
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a signed int, returns 0 otherwise.
 */
_Data_['getINT'] = function() {
    return _pn_data_get_int(this._data);
};

/**
 * @method getCHAR
 * @memberof! proton.Data#
 * @returns {string} the character represented by the unicode value of the current node.
 */
_Data_['getCHAR'] = function() {
    return String.fromCharCode(_pn_data_get_char(this._data));
};

/**
 * Retrieve an unsigned long value. N.B. large values can suffer from a loss of
 * precision as JavaScript numbers are restricted to 64 bit double values.
 * @method getULONG
 * @memberof! proton.Data#
 * @returns {proton.Data.Long} value if the current node is an unsigned long, returns 0 otherwise.
 */
_Data_['getULONG'] = function() {
    var low = _pn_data_get_ulong(this._data);
    var high = Runtime.getTempRet0();
    var long = new Data.Long(low, high);
    long = long.toNumber();
    return (long >= 0) ? long : Data.Long.TWO_PWR_64_DBL_ + long;
};

/**
 * Retrieve a signed long value. N.B. large values can suffer from a loss of
 * precision as JavaScript numbers are restricted to 64 bit double values.
 * @method getLONG
 * @memberof! proton.Data#
 * @returns {proton.Data.Long} value if the current node is a signed long, returns 0 otherwise.
 */
_Data_['getLONG'] = function() {
    // Getting the long is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to hold
    // the 64 bit number and Data.Long.toNumber() to convert it back into a
    // JavaScript number.
    var low = _pn_data_get_long(this._data);
    var high = Runtime.getTempRet0();
    var long = new Data.Long(low, high);
    long = long.toNumber();
    return long;
};

/**
 * @method getTIMESTAMP
 * @memberof! proton.Data#
 * @returns {Date} a native JavaScript Date instance representing the timestamp.
 */
_Data_['getTIMESTAMP'] = function() {
    // Getting the timestamp is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to hold
    // the 64 bit number and Data.Long.toNumber() to convert it back into a
    // JavaScript number.
    var low =  _pn_data_get_timestamp(this._data);
    var high = Runtime.getTempRet0();
    var long = new Data.Long(low, high);
    long = long.toNumber();
    return new Date(long);
};

/**
 * Retrieves a  float value. N.B. converting between floats and doubles is imprecise
 * so the resulting value might not quite be what you expect.
 * @method getFLOAT
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a float, returns 0 otherwise.
 */
_Data_['getFLOAT'] = function() {
    return _pn_data_get_float(this._data);
};

/**
 * @method getDOUBLE
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a double, returns 0 otherwise.
 */
_Data_['getDOUBLE'] = function() {
    return _pn_data_get_double(this._data);
};

/**
 * @method getDECIMAL32
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a decimal32, returns 0 otherwise.
 */
_Data_['getDECIMAL32'] = function() {
console.log("getDECIMAL32 not properly implemented yet");
    return _pn_data_get_decimal32(this._data);
};

/**
 * @method getDECIMAL64
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a decimal64, returns 0 otherwise.
 */
_Data_['getDECIMAL64'] = function() {
console.log("getDECIMAL64 not properly implemented yet");
    return _pn_data_get_decimal64(this._data);
};

/**
 * @method getDECIMAL128
 * @memberof! proton.Data#
 * @returns {number} value if the current node is a decimal128, returns 0 otherwise.
 */
_Data_['getDECIMAL128'] = function() {
console.log("getDECIMAL128 not properly implemented yet");
    return _pn_data_get_decimal128(this._data);
};

/**
 * @method getUUID
 * @memberof! proton.Data#
 * @returns {proton.Data.Uuid} value if the current node is a UUID, returns null otherwise.
 */
_Data_['getUUID'] = function() {
    var sp = Runtime.stackSave();

    // Here's the quirky bit, pn_data_get_uuid actually returns pn_uuid_t
    // *by value* but the low-level code handles this *by pointer* so we first
    // need to allocate 16 bytes storage for pn_uuid_t on the emscripten stack
    // and then we pass the pointer to that storage as the first parameter to the
    // compiled pn_data_get_uuid.
    var bytes = allocate(16, 'i8', ALLOC_STACK); // pn_uuid_t is 16 bytes.
    _pn_data_get_uuid(bytes, this._data);

    // Create a new UUID from the bytes
    var uuid = new Data['Uuid'](bytes);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    return uuid;
};

/**
 * @method getBINARY
 * @memberof! proton.Data#
 * @returns {proton.Data.Binary} value if the current node is a Binary, returns null otherwise.
 */
_Data_['getBINARY'] = function() {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_bytes bytes = pn_data_get_binary(data);

    // Here's the quirky bit, pn_data_get_binary actually returns pn_bytes_t 
    // *by value* but the low-level code handles this *by pointer* so we first
    // need to allocate 8 bytes storage for {size, start} on the emscripten stack
    // and then we pass the pointer to that storage as the first parameter to the
    // compiled pn_data_get_binary.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_data_get_binary(bytes, this._data);

    // The bytes variable is really of type pn_bytes_t* so we use emscripten's
    // getValue() call to retrieve the size and then the start pointer.
    var size  = getValue(bytes, 'i32');
    var start = getValue(bytes + 4, '*');

    // Create a proton.Data.Binary from the pn_bytes_t information.
    var binary = new Data['Binary'](size, start);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    // If _decodeBinaryAsString is set return the stringified form of the Binary.
    if (this._decodeBinaryAsString) {
        return binary.toString();
    } else {
        return binary;
    }
};

/**
 * Gets a unicode String value from the current node.
 * @method getSTRING
 * @memberof! proton.Data#
 * @returns {string} value if the current node is a String, returns "" otherwise.
 */
_Data_['getSTRING'] = function() {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_bytes bytes = pn_data_get_string(data);

    // Here's the quirky bit, pn_data_get_string actually returns pn_bytes_t 
    // *by value* but the low-level code handles this *by pointer* so we first
    // need to allocate 8 bytes storage for {size, start} on the emscripten stack
    // and then we pass the pointer to that storage as the first parameter to the
    // compiled pn_data_get_string.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_data_get_string(bytes, this._data);

    // The bytes variable is really of type pn_bytes_t* so we use emscripten's
    // getValue() call to retrieve the size and then the start pointer.
    var size  = getValue(bytes, 'i32');
    var start = getValue(bytes + 4, '*');

    // Create a native JavaScript String from the pn_bytes_t information.
    var string = Pointer_stringify(start, size);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    return string;
};

/**
 * Gets a symbolic value. According to the AMQP 1.0 Specification Symbols are
 * values from a constrained domain. Although the set of possible domains is
 * open-ended, typically the both number and size of symbols in use for any
 * given application will be small, e.g. small enough that it is reasonable to
 * cache all the distinct values. Symbols are encoded as ASCII characters.
 * @method getSYMBOL
 * @memberof! proton.Data#
 * @returns {proton.Data.Symbol} value if the current node is a Symbol, returns "" otherwise.
 */
_Data_['getSYMBOL'] = function() {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_bytes bytes = pn_data_get_symbol(data);

    // Here's the quirky bit, pn_data_get_symbol actually returns pn_bytes_t 
    // *by value* but the low-level code handles this *by pointer* so we first
    // need to allocate 8 bytes storage for {size, start} on the emscripten stack
    // and then we pass the pointer to that storage as the first parameter to the
    // compiled pn_data_get_symbol.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_data_get_symbol(bytes, this._data);

    // The bytes variable is really of type pn_bytes_t* so we use emscripten's
    // getValue() call to retrieve the size and then the start pointer.
    var size  = getValue(bytes, 'i32');
    var start = getValue(bytes + 4, '*');

    // Create a native JavaScript String from the pn_bytes_t information.
    var string = Pointer_stringify(start, size);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    return new Data['Symbol'](string);
};

/**
 * Performs a deep copy of the current {@link proton.Data} instance and returns it
 * @method copy
 * @memberof! proton.Data#
 * @returns {proton.Data} a copy of the current {@link proton.Data} instance.
 */
_Data_['copy'] = function() {
    var copy = new Data();
    this._check(_pn_data_copy(copy._data, this._data));
    return copy;
};

/**
 * Format the encoded AMQP Data into a string representation and return it.
 * @method format
 * @memberof! proton.Data#
 * @returns {string} a formatted string representation of the encoded Data.
 */
_Data_['format'] = function() {
    var size = 1024; // Pass by reference variable - need to use setValue to initialise it.
    while (true) {
        setValue(size, size, 'i32'); // Set pass by reference variable.
        var bytes = _malloc(size);   // Allocate storage from emscripten heap.
        var err = _pn_data_format(this._data, bytes, size);
        var size = getValue(size, 'i32'); // Dereference the real size value;

        if (err === Module['Error']['OVERFLOW']) {
            _free(bytes);
            size *= 2;
        } else {
            var string = Pointer_stringify(bytes);
            _free(bytes);
            this._check(err)
            return string;
        }
    }
};

/**
 * Print the internal state of the {@link proton.Data} in human readable form.
 * TODO. This seems to "crash" if compound nodes such as DESCRIBED, MAP or LIST
 * are present in the tree, this is most likely a problem with the underlying C
 * implementation as all the other navigation and format methods work - need to
 * check by testing with some native C code.
 * @method dump
 * @memberof! proton.Data#
 */
_Data_['dump'] = function() {
    _pn_data_dump(this._data);
};

/**
 * Serialise a Native JavaScript Object into an AMQP Map.
 * @method putMAP
 * @memberof! proton.Data#
 * @param {object} object the Native JavaScript Object that we wish to serialise.
 */
_Data_['putMAP'] = function(object) {
    this['putMAPNODE']();
    this['enter']();
    for (var key in object) {
        if (object.hasOwnProperty(key)) {
            this['putObject'](key);
            this['putObject'](object[key]);
        }
    }
    this['exit']();
};

/**
 * Deserialise from an AMQP Map into a Native JavaScript Object.
 * @method getMAP
 * @memberof! proton.Data#
 * @returns {object} the deserialised Native JavaScript Object.
 */
_Data_['getMAP'] = function() {
    if (this['enter']()) {
        var result = {};
        while (this['next']()) {
            var key = this['getObject']();
            var value = null;
            if (this['next']()) {
                value = this['getObject']();
            }
            result[key] = value;
        }
        this['exit']();
        return result;
    }
};

/**
 * Serialise a Native JavaScript Array into an AMQP List.
 * @method putLIST
 * @memberof! proton.Data#
 * @param {Array} array the Native JavaScript Array that we wish to serialise.
 */
_Data_['putLIST'] = function(array) {
    this['putLISTNODE']();
    this['enter']();
    for (var i = 0, len = array.length; i < len; i++) {
        this['putObject'](array[i]);
    }
    this['exit']();
};

/**
 * Deserialise from an AMQP List into a Native JavaScript Array.
 * @method getLIST
 * @memberof! proton.Data#
 * @returns {Array} the deserialised Native JavaScript Array.
 */
_Data_['getLIST'] = function() {
    if (this['enter']()) {
        var result = [];
        while (this['next']()) {
            result.push(this['getObject']());
        }
        this['exit']();
        return result;
    }
};

/**
 * Serialise a proton.Data.Described into an AMQP Described.
 * @method putDESCRIBED
 * @memberof! proton.Data#
 * @param {proton.Data.Described} d the proton.Data.Described that we wish to serialise.
 */
_Data_['putDESCRIBED'] = function(d) {
    this['putDESCRIBEDNODE']();
    this['enter']();
    this['putObject'](d['descriptor']);
    this['putObject'](d['value']);
    this['exit']();
};

/**
 * Deserialise from an AMQP Described into a proton.Data.Described.
 * @method getDESCRIBED
 * @memberof! proton.Data#
 * @returns {proton.Data.Described} the deserialised proton.Data.Described.
 */
_Data_['getDESCRIBED'] = function() {
    if (this['enter']()) {
        this['next']();
        var descriptor = this['getObject']();
        this['next']();
        var value = this['getObject']();
        this['exit']();
        return new Data['Described'](value, descriptor);
    }
};

/**
 * Serialise a proton.Data.Array or JavaScript TypedArray into an AMQP Array.
 * @method putARRAY
 * @memberof! proton.Data#
 * @param {object} a the proton.Data.Array or TypedArray that we wish to serialise.
 */
_Data_['putARRAY'] = function(a) {
    var type = 1;
    var descriptor = 'TypedArray';
    var array = a;

    if (a instanceof Data['Array']) { // Array is a proton.Data.Array
        type = Data[a['type']]; // Find the integer type from its name string.
        descriptor = a['descriptor'];
        array = a['elements'];
    } else { // Array is a Native JavaScript TypedArray so work out the right type.
        if (a instanceof Int8Array) {
            type = Data['BYTE'];
        } else if (a instanceof Uint8Array || a instanceof Uint8ClampedArray) {
            type = Data['UBYTE'];
        } else if (a instanceof Int16Array) {
            type = Data['SHORT'];
        } else if (a instanceof Uint16Array) {
            type = Data['USHORT'];
        } else if (a instanceof Int32Array) {
            type = Data['INT'];
        } else if (a instanceof Uint32Array) {
            type = Data['UINT'];
        } else if (a instanceof Float32Array) {
            type = Data['FLOAT'];
        } else if (a instanceof Float64Array) {
            type = Data['DOUBLE'];
        }
    }

    var described = descriptor != null;

    this['putARRAYNODE'](described, type);
    this['enter']();
    if (described) {
        this['putObject'](descriptor);
    }
    var putter = 'put' + Data['TypeNames'][type];
    for (var i = 0, len = array.length; i < len; i++) {
        var value = array[i];
        value = (value instanceof Data.TypedNumber) ? value.value : value;
        this[putter](value);
    }
    this['exit']();
};

/**
 * Deserialise from an AMQP Array into a proton.Data.Array.
 * @method getARRAY
 * @memberof! proton.Data#
 * @returns {proton.Data.Array} the deserialised proton.Data.Array.
 */
_Data_['getARRAY'] = function() {
    var metadata = this['getARRAYNODE']();
    var count = metadata['count'];
    var described = metadata['described'];
    var type = metadata['type'];

    if (type === null) {
        return null;
    }

    var elements = null;
    if (typeof ArrayBuffer === 'function') {
        if (type === Data['BYTE']) {
            elements = new Int8Array(count);
        } else if (type === Data['UBYTE']) {
            elements = new Uint8Array(count);
        } else if (type === Data['SHORT']) {
            elements = new Int16Array(count);
        } else if (type === Data['USHORT']) {
            elements = new Uint16Array(count);
        } else if (type === Data['INT']) {
            elements = new Int32Array(count);
        } else if (type === Data['UINT']) {
            elements = new Uint32Array(count);
        } else if (type === Data['FLOAT']) {
            elements = new Float32Array(count);
        } else if (type === Data['DOUBLE']) {
            elements = new Float64Array(count);
        } else {
            elements = new Array(count);
        }
    } else {
        elements = new Array(count);
    }

    if (this['enter']()) {
        var descriptor; // Deliberately initialised as undefined not null.
        if (described) {
            this['next']();
            descriptor = this['getObject']();
        }

        for (var i = 0; i < count; i++) {
            this['next']();
            elements[i] = this['getObject']();
        }

        this['exit']();
        if (descriptor === 'TypedArray') {
            return elements;
        } else {
            return new Data['Array'](type, elements, descriptor);
        }
    }
};

/**
 * This method is the entry point for serialising native JavaScript types into
 * AMQP types. In an ideal world there would be a nice clean one to one mapping
 * and we could employ a look-up table but in practice the JavaScript type system
 * doesn't really lend itself to that and we have to employ extra checks,
 * heuristics and inferences.
 * @method putObject
 * @memberof! proton.Data#
 * @param {object} obj the JavaScript Object or primitive to be serialised.
 */
_Data_['putObject'] = function(obj) {
//console.log("Data.putObject " + obj);

    if (obj == null) { // == Checks for null and undefined.
        this['putNULL']();
    } else if (Data.isString(obj)) {
        var quoted = obj.match(/(['"])[^'"]*\1/);
        if (quoted) { // If a quoted string extract the string inside the quotes.
            obj = quoted[0].slice(1, -1);
        }
        this['putSTRING'](obj);
    } else if (obj instanceof Date) {
        this['putTIMESTAMP'](obj);
    } else if (obj instanceof Data['Uuid']) {
        this['putUUID'](obj);
    } else if (obj instanceof Data['Binary']) {
        this['putBINARY'](obj);
    } else if (obj instanceof Data['Symbol']) {
        this['putSYMBOL'](obj);
    } else if (obj instanceof Data['Described']) {
        this['putDESCRIBED'](obj);
    } else if (obj instanceof Data['Array']) {
        this['putARRAY'](obj);
    } else if (obj.buffer && (typeof ArrayBuffer === 'function') && 
               obj.buffer instanceof ArrayBuffer) {
        this['putARRAY'](obj);
    } else if (obj instanceof Data.TypedNumber) { // Dot notation used for "protected" inner class.
        // Call the appropriate serialisation method based upon the numerical type.
        this['put' + obj.type](obj.value);
    } else if (Data.isNumber(obj)) {
        /**
         * This block encodes standard JavaScript numbers by making some inferences.
         * Encoding JavaScript numbers is surprisingly complex and has several
         * gotchas. The code here tries to do what the author believes is the
         * most "intuitive" encoding of the native JavaScript Number. It first
         * tries to identify if the number is an integer or floating point type
         * by checking if the number modulo 1 is zero (i.e. if it has a remainder
         * then it's a floating point type, which is encoded here as a double).
         * If the number is an integer type a test is made to check if it is a
         * 32 bit Int value. N.B. gotcha - JavaScript automagically coerces floating
         * point numbers with a zero Fractional Part into an *exact* integer so
         * numbers like 1.0, 100.0 etc. will be encoded as int or long here,
         * which is unlikely to be what is wanted. There's no easy "transparent"
         * way around this. The TypedNumber approach above allows applications
         * to express more explicitly what is required, for example (1.0).float()
         * (1).ubyte(), (5).long() etc.
         */
        if (obj % 1 === 0) {
            if (obj === (obj|0)) { // the |0 coerces to a 32 bit value.
                // 32 bit integer - encode as an INT.
                this['putINT'](obj);
            } else { // Longer than 32 bit - encode as a Long.
                this['putLONG'](obj);
            }
        } else { // Floating point type - encode as a Double
            this['putDOUBLE'](obj);
        }
    } else if (Data.isBoolean(obj)) {
        this['putBOOL'](obj);
    } else if (Data.isArray(obj)) { // Native JavaScript Array
        this['putLIST'](obj);
    } else {
        this['putMAP'](obj);
    }
};

/**
 * @method getObject
 * @memberof! proton.Data#
 * @returns {object} the JavaScript Object or primitive being deserialised.
 */
_Data_['getObject'] = function() {
    var type = Data['TypeNames'][this.type()];
    type = type ? type : 'NULL';
    var getter = 'get' + type;
    return this[getter]();
};

