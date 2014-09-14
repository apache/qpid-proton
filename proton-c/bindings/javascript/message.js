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
/*                                  Message                                  */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.Message instance.
 * @classdesc This class is a mutable holder of message content that may be used
 * to generate and encode or decode and access AMQP formatted message data.
 * @constructor proton.Message
 * @property {object} instructions delivery instructions for the message.
 * @property {object} annotations infrastructure defined message annotations.
 * @property {object} properties application defined message properties.
 * @property {object} body message body as a native JavaScript Object.
 * @property {object} data message body as a proton.Data Object.
 */
Module['Message'] = function() { // Message Constructor.
    this._message = _pn_message();
    this._id = new Data(_pn_message_id(this._message));
    this._correlationId = new Data(_pn_message_correlation_id(this._message));

    // ************************* Public properties ****************************

    this['instructions'] = null;
    this['annotations'] = null;

    // Intitialise with an empty Object so we can set properties in a natural way.
    // message.properties.prop1 = "foo";
    // message.properties.prop2 = "bar";
    this['properties'] = {};

    this['body'] = null;
    this['data'] = null;
};

// Expose constructor as package scope variable to make internal calls less verbose.
var Message = Module['Message'];

// Expose prototype as a variable to make method declarations less verbose.
var _Message_ = Message.prototype;

// ************************** Class properties ********************************

Message['DEFAULT_PRIORITY'] = 4; /** Default priority for messages.*/

// ************************* Protected methods ********************************

// We use the dot notation rather than associative array form for protected
// methods so they are visible to this "package", but the Closure compiler will
// minify and obfuscate names, effectively making a defacto "protected" method.

/**
 * This helper method checks the supplied error code, converts it into an
 * exception and throws the exception. This method will try to use the message
 * populated in pn_message_error(), if present, but if not it will fall
 * back to using the basic error code rendering from pn_code().
 * @param code the error code to check.
 */
_Message_._check = function(code) {
    if (code < 0) {
        var errno = this['getErrno']();
        var message = errno ? this['getError']() : Pointer_stringify(_pn_code(code));

        throw new Module['MessageError'](message);
    } else {
        return code;
    }
};

/**
 * Encode the Message prior to sending on the wire.
 */
_Message_._preEncode = function() {
    // A Message Object may be reused so we create new Data instances and clear
    // the state for them each time put() gets called.
    var inst = new Data(_pn_message_instructions(this._message));
    var ann = new Data(_pn_message_annotations(this._message));
    var props = new Data(_pn_message_properties(this._message));
    var body = new Data(_pn_message_body(this._message));

    inst.clear();
    if (this['instructions']) {
        inst['putObject'](this['instructions']);
    }

    ann.clear();
    if (this['annotations']) {
        ann['putObject'](this['annotations']);
    }

    props.clear();
    if (this['properties']) {
        props['putObject'](this['properties']);
    }

    body.clear();
    if (this['body']) {
        var contentType = this['getContentType']();
        if (contentType) {
            var value = this['body'];
            if (contentType === 'application/json' && JSON) { // Optionally encode body as JSON.
                var json = JSON.stringify(value);
                value = new Data['Binary'](json);
            } else if (!(value instanceof Data['Binary'])) { // Construct a Binary from the body
                value = new Data['Binary'](value);
            }
            // As content-type is set we send as an opaque AMQP data section.
            this['setInferred'](true);
            body['putBINARY'](value);
        } else { // By default encode body using the native AMQP type system.
            this['setInferred'](false);
            body['putObject'](this['body']);
        }
    }
};

/**
 * Decode the Message after receiving off the wire.
 * @param {boolean} decodeBinaryAsString if set decode any AMQP Binary payload
 *        objects as strings. This can be useful as the data in Binary objects
 *        will be overwritten with subsequent calls to get, so they must be
 *        explicitly copied. Needless to say it is only safe to set this flag if
 *        you know that the data you are dealing with is actually a string, for
 *        example C/C++ applications often seem to encode strings as AMQP binary,
 *        a common cause of interoperability problems.
 */
_Message_._postDecode = function(decodeBinaryAsString) {
    var inst = new Data(_pn_message_instructions(this._message));
    var ann = new Data(_pn_message_annotations(this._message));
    var props = new Data(_pn_message_properties(this._message));
    var body = new Data(_pn_message_body(this._message), decodeBinaryAsString);

    if (inst.next()) {
        this['instructions'] = inst['getObject']();
    } else {
        this['instructions'] = {};
    }

    if (ann.next()) {
        this['annotations'] = ann['getObject']();
    } else {
        this['annotations'] = {};
    }

    if (props.next()) {
        this['properties'] = props['getObject']();
    } else {
        this['properties'] = {};
    }

    if (body.next()) {
        this['data'] = body;
        this['body'] = body['getObject']();
        var contentType = this['getContentType']();
        if (contentType) {
            if (contentType === 'application/json' && JSON) {
                var json = this['body'].toString(); // Convert Binary to String.
                this['body'] = JSON.parse(json);
            } else if (contentType.indexOf('text/') === 0) { // It's a text/* MIME type
                this['body'] = this['body'].toString(); // Convert Binary to String.
            }
        }
    } else {
        this['data'] = null;
        this['body'] = null;
    }
};

// *************************** Public methods *********************************

/**
 * Free the Message.
 * <p>
 * N.B. This method has to be called explicitly in JavaScript as we can't
 * intercept finalisers, so we need to remember to free before removing refs.
 * @method free
 * @memberof! proton.Message#
 */
_Message_['free'] = function() {
    _pn_message_free(this._message);
};

/**
 * @method getErrno
 * @memberof! proton.Message#
 * @returns {number the most recent error message code.
 */
_Message_['getErrno'] = function() {
    return _pn_message_errno(this._message);
};

/**
 * @method getError
 * @memberof! proton.Message#
 * @returns {string} the most recent error message as a String.
 */
_Message_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_message_error(this._message)));
};

/**
 * Clears the contents of the Message. All fields will be reset to their default values.
 * @method clear
 * @memberof! proton.Message#
 */
_Message_['clear'] = function() {
    _pn_message_clear(this._message);
    this['instructions'] = null;
    this['annotations'] = null;
    this['properties'] = {};
    this['body'] = null;
    this['data'] = null;
};

/**
 * Get the inferred flag for a message.
 * <p>
 * The inferred flag for a message indicates how the message content
 * is encoded into AMQP sections. If inferred is true then binary and
 * list values in the body of the message will be encoded as AMQP DATA
 * and AMQP SEQUENCE sections, respectively. If inferred is false,
 * then all values in the body of the message will be encoded as AMQP
 * VALUE sections regardless of their type. Use
 * {@link proton.Message.setInferred} to set the value.
 * @method isInferred
 * @memberof! proton.Message#
 * @returns {boolean} true iff the inferred flag for the message is set.
 */
_Message_['isInferred'] = function() {
    return (_pn_message_is_inferred(this._message) > 0);
};

/**
 * Set the inferred flag for a message. See {@link proton.Message.isInferred} 
 * for a description of what the inferred flag is.
 * @method setInferred
 * @memberof! proton.Message#
 * @param {boolean} inferred the new value of the inferred flag.
 */
_Message_['setInferred'] = function(inferred) {
    this._check(_pn_message_set_inferred(this._message, inferred));
};

/**
 * Get the durable flag for a message.
 * <p>
 * The durable flag indicates that any parties taking responsibility
 * for the message must durably store the content. Use
 * {@link proton.Message.setDurable} to set the value.
 * @method isDurable
 * @memberof! proton.Message#
 * @returns {boolean} true iff the durable flag for the message is set.
 */
_Message_['isDurable'] = function() {
    return (_pn_message_is_durable(this._message) > 0);
};

/**
 * Set the durable flag for a message. See {@link proton.Message.isDurable} 
 * for a description of what the durable flag is.
 * @method setDurable
 * @memberof! proton.Message#
 * @param {boolean} durable the new value of the durable flag.
 */
_Message_['setDurable'] = function(durable) {
    this._check(_pn_message_set_durable(this._message, durable));
};

/**
 * Get the priority for a message.
 * <p>
 * The priority of a message impacts ordering guarantees. Within a
 * given ordered context, higher priority messages may jump ahead of
 * lower priority messages. Priority range is 0..255
 * @method getPriority
 * @memberof! proton.Message#
 * @returns {number} the priority of the Message.
 */
_Message_['getPriority'] = function() {
    return _pn_message_get_priority(this._message) & 0xFF; // & 0xFF converts to unsigned.
};

/**
 * Set the priority of the Message. See {@link proton.Message.getPriority}
 * for details on message priority.
 * @method setPriority
 * @memberof! proton.Message#
 * @param {number} priority the address we want to send the Message to.
 */
_Message_['setPriority'] = function(priority) {
    this._check(_pn_message_set_priority(this._message, priority));
};

/**
 * Get the ttl for a message.
 * <p>
 * The ttl for a message determines how long a message is considered
 * live. When a message is held for retransmit, the ttl is
 * decremented. Once the ttl reaches zero, the message is considered
 * dead. Once a message is considered dead it may be dropped. Use
 * {@link proton.Message.setTTL} to set the ttl for a message.
 * @method getTTL
 * @memberof! proton.Message#
 * @returns {number} the ttl in milliseconds.
 */
_Message_['getTTL'] = function() {
    return _pn_message_get_ttl(this._message);
};

/**
 * Set the ttl for a message. See {@link proton.Message.getTTL}
 * for a detailed description of message ttl.
 * @method setTTL
 * @memberof! proton.Message#
 * @param {number} ttl the new value for the message ttl in milliseconds.
 */
_Message_['setTTL'] = function(ttl) {
    this._check(_pn_message_set_ttl(this._message, ttl));
};

/**
 * Get the first acquirer flag for a message.
 * <p>
 * When set to true, the first acquirer flag for a message indicates
 * that the recipient of the message is the first recipient to acquire
 * the message, i.e. there have been no failed delivery attempts to
 * other acquirers. Note that this does not mean the message has not
 * been delivered to, but not acquired, by other recipients.
 * @method isFirstAcquirer
 * @memberof! proton.Message#
 * @returns {boolean} true iff the first acquirer flag for the message is set.
 */
_Message_['isFirstAcquirer'] = function() {
    return (_pn_message_is_first_acquirer(this._message) > 0);
};

/**
 * Set the first acquirer flag for a message. See {@link proton.Message.isFirstAcquirer} 
 * for details on the first acquirer flag.
 * @method setFirstAcquirer
 * @memberof! proton.Message#
 * @param {boolean} first the new value of the first acquirer flag.
 */
_Message_['setFirstAcquirer'] = function(first) {
    this._check(_pn_message_set_first_acquirer(this._message, first));
};

/**
 * Get the delivery count for a message.
 * <p>
 * The delivery count field tracks how many attempts have been made to
 * deliver a message. Use {@link proton.Message.setDeliveryCount} to set
 * the delivery count for a message.
 * @method getDeliveryCount
 * @memberof! proton.Message#
 * @returns {number} the delivery count for the message.
 */
_Message_['getDeliveryCount'] = function() {
    return _pn_message_get_delivery_count(this._message);
};

/**
 * Set the delivery count for a message. See {@link proton.Message.getDeliveryCount}
 * for details on what the delivery count means.
 * @method setDeliveryCount
 * @memberof! proton.Message#
 * @param {number} count the new delivery count.
 */
_Message_['setDeliveryCount'] = function(count) {
    this._check(_pn_message_set_delivery_count(this._message, count));
};

/**
 * Get the id for a message.
 * <p>
 * The message id provides a globally unique identifier for a message.
 * A message id can be an a string, an unsigned long, a uuid or a binary value.
 * @method getID
 * @memberof! proton.Message#
 * @returns {(number|string|proton.Data.Long|proton.Data.Uuid|proton.Data.Binary)} the message id.
 */
_Message_['getID'] = function() {
    return this._id['getObject']();
};

/**
 * Set the id for a message. See {@link proton.Message.getID}
 * for more details on the meaning of the message id. Note that only string,
 * unsigned long, uuid, or binary values are permitted.
 * @method setID
 * @memberof! proton.Message#
 * @param {(number|string|proton.Data.Long|proton.Data.Uuid|proton.Data.Binary)} id the
 *        new value of the message id.
 */
_Message_['setID'] = function(id) {
    this._id['rewind']();
    if (Data.isNumber(id)) {
        this._id['putULONG'](id);
    } else {
        this._id['putObject'](id);
    }
};

/**
 * Get the user id of the message creator.
 * <p>
 * The underlying raw data of the returned {@link proton.Data.Binary} will be
 * valid until any one of the following operations occur:
 * <pre>
 *  - {@link proton.Message.free}
 *  - {@link proton.Message.clear}
 *  - {@link proton.Message.setUserID}
 * </pre>
 * @method getUserID
 * @memberof! proton.Message#
 * @returns {proton.Data.Binary} the message's user id.
 */
_Message_['getUserID'] = function() {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_bytes_t bytes = pn_message_get_user_id(message);

    // Here's the quirky bit, pn_message_get_user_id actually returns pn_bytes_t 
    // *by value* but the low-level code handles this *by pointer* so we first
    // need to allocate 8 bytes storage for {size, start} on the emscripten stack
    // and then we pass the pointer to that storage as the first parameter to the
    // compiled pn_message_get_user_id.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_message_get_user_id(bytes, this._message);

    // The bytes variable is really of type pn_bytes_t* so we use emscripten's
    // getValue() call to retrieve the size and then the start pointer.
    var size  = getValue(bytes, 'i32');
    var start = getValue(bytes + 4, '*');

    // Create a proton.Data.Binary from the pn_bytes_t information.
    var binary = new Data['Binary'](size, start);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    return binary;
};

/**
 * Set the user id for a message. This method takes a {@link proton.Data.Binary}
 * consuming the underlying raw data in the process. For convenience this method
 * also accepts a {@link proton.Data.Uuid}, number or string, converting them to a
 * Binary internally. N.B. getUserID always returns a {@link proton.Data.Binary}
 * even if a string or {@link proton.Data.Uuid} has been passed to setUserID.
 * @method setUserID
 * @memberof! proton.Message#
 * @param {(string|proton.Data.Uuid)} id the new user id for the message.
 */
_Message_['setUserID'] = function(id) {
    // If the id parameter is a proton.Data.Binary use it otherwise create a Binary
    // using the string form of the parameter that was passed.
    id = (id instanceof Data['Binary']) ? id : new Data['Binary']('' + id);

    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_message_set_user_id(message, pn_bytes(id.size, id.start));

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the pn_bytes.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, id.size, id.start);

    // The compiled pn_message_set_user_id takes the pn_bytes_t by reference not value.
    this._check(_pn_message_set_user_id(this._message, bytes));

    // After calling _pn_message_set_user_id the underlying Message object "owns" the
    // binary data, so we can call free on the proton.Data.Binary instance to
    // release any storage it has acquired back to the emscripten heap.
    id['free']();
    Runtime.stackRestore(sp);
};

/**
 * Get the address for a message.
 * @method getAddress
 * @memberof! proton.Message#
 * @returns {string} the address of the Message.
 */
_Message_['getAddress'] = function() {
    return Pointer_stringify(_pn_message_get_address(this._message));
};

/**
 * Set the address of the Message.
 * @method setAddress
 * @memberof! proton.Message#
 * @param {string} address the address we want to send the Message to.
 */
_Message_['setAddress'] = function(address) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_address(this._message, allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the subject for a message.
 * @method getSubject
 * @memberof! proton.Message#
 * @returns {string} the subject of the Message.
 */
_Message_['getSubject'] = function() {
    return Pointer_stringify(_pn_message_get_subject(this._message));
};

/**
 * Set the subject of the Message.
 * @method setSubject
 * @memberof! proton.Message#
 * @param {string} subject the subject we want to set for the Message.
 */
_Message_['setSubject'] = function(subject) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_subject(this._message, allocate(intArrayFromString(subject), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the reply to for a message.
 * @method getReplyTo
 * @memberof! proton.Message#
 * @returns {string} the reply to of the Message.
 */
_Message_['getReplyTo'] = function() {
    return Pointer_stringify(_pn_message_get_reply_to(this._message));
};

/**
 * Set the reply to for a message.
 * @method setReplyTo
 * @memberof! proton.Message#
 * @param {string} reply the reply to we want to set for the Message.
 */
_Message_['setReplyTo'] = function(reply) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_reply_to(this._message, allocate(intArrayFromString(reply), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the correlation id for a message.
 * <p>
 * A correlation id can be an a string, an unsigned long, a uuid or a binary value.
 * @method getCorrelationID
 * @memberof! proton.Message#
 * @returns {(number|string|proton.Data.Long|proton.Data.Uuid|proton.Data.Binary)} the message id.
 */
_Message_['getCorrelationID'] = function() {
    return this._correlationId['getObject']();
};

/**
 * Set the correlation id for a message. See {@link proton.Message.getCorrelationID}
 * for more details on the meaning of the correlation id. Note that only string,
 * unsigned long, uuid, or binary values are permitted.
 * @method setCorrelationID
 * @memberof! proton.Message#
 * @param {(number|string|proton.Data.Long|proton.Data.Uuid|proton.Data.Binary)} id the
 *        new value of the correlation id.
 */
_Message_['setCorrelationID'] = function(id) {
    this._correlationId['rewind']();
    if (Data.isNumber(id)) {
        this._correlationId['putULONG'](id);
    } else {
        this._correlationId['putObject'](id);
    }
};

/**
 * Get the content type for a message.
 * @method getContentType
 * @memberof! proton.Message#
 * @returns {string} the content type of the Message.
 */
_Message_['getContentType'] = function() {
    return Pointer_stringify(_pn_message_get_content_type(this._message));
};

/**
 * Set the content type for a message.
 * @method setContentType
 * @memberof! proton.Message#
 * @param {string} type the content type we want to set for the Message.
 */
_Message_['setContentType'] = function(type) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_content_type(this._message, allocate(intArrayFromString(type), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the content encoding for a message.
 * @method getContentEncoding
 * @memberof! proton.Message#
 * @returns {string} the content encoding of the Message.
 */
_Message_['getContentEncoding'] = function() {
    return Pointer_stringify(_pn_message_get_content_encoding(this._message));
};

/**
 * Set the content encoding for a message.
 * @method setContentEncoding
 * @memberof! proton.Message#
 * @param {string} encoding the content encoding we want to set for the Message.
 */
_Message_['setContentEncoding'] = function(encoding) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_content_encoding(this._message, allocate(intArrayFromString(encoding), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the expiry time for a message.
 * A zero value for the expiry time indicates that the message will
 * never expire. This is the default value.
 * @method getExpiryTime
 * @memberof! proton.Message#
 * @returns {Date} the expiry time for the message.
 */
_Message_['getExpiryTime'] = function() {
    // Getting the timestamp is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to hold
    // the 64 bit number and Data.Long.toNumber() to convert it back into a
    // JavaScript number.
    var low =  _pn_message_get_expiry_time(this._message);
    var high = Runtime.getTempRet0();
    var long = new Data.Long(low, high);
    long = long.toNumber();
    return new Date(long);
};

/**
 * Set the expiry time for a message.
 * @method setExpiryTime
 * @memberof! proton.Message#
 * @param {(number|Date)} time the new expiry time for the message.
 */
_Message_['setExpiryTime'] = function(time) {
    // Note that a timestamp is a 64 bit number so we have to use a proton.Data.Long.
    var timestamp = Data.Long.fromNumber(time.valueOf());
    this._check(_pn_message_set_expiry_time(this._message, timestamp.getLowBitsUnsigned(), timestamp.getHighBits()));
};

/**
 * Get the creation time for a message.
 * A zero value for the creation time indicates that the creation time
 * has not been set. This is the default value.
 * @method getCreationTime
 * @memberof! proton.Message#
 * @returns {Date} the creation time for the message.
 */
_Message_['getCreationTime'] = function() {
    // Getting the timestamp is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to hold
    // the 64 bit number and Data.Long.toNumber() to convert it back into a
    // JavaScript number.
    var low =  _pn_message_get_creation_time(this._message);
    var high = Runtime.getTempRet0();
    var long = new Data.Long(low, high);
    long = long.toNumber();
    return new Date(long);
};

/**
 * Set the creation time for a message.
 * @method setCreationTime
 * @memberof! proton.Message#
 * @param {(number|Date)} time the new creation time for the message.
 */
_Message_['setCreationTime'] = function(time) {
    // Note that a timestamp is a 64 bit number so we have to use a proton.Data.Long.
    var timestamp = Data.Long.fromNumber(time.valueOf());
    this._check(_pn_message_set_creation_time(this._message, timestamp.getLowBitsUnsigned(), timestamp.getHighBits()));
};

/**
 * Get the group id for a message.
 * @method getGroupID
 * @memberof! proton.Message#
 * @returns {string} the group id of the Message.
 */
_Message_['getGroupID'] = function() {
    return Pointer_stringify(_pn_message_get_group_id(this._message));
};

/**
 * Set the group id for a message.
 * @method setGroupID
 * @memberof! proton.Message#
 * @param {string} id the group id we want to set for the Message.
 */
_Message_['setGroupID'] = function(id) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_group_id(this._message, allocate(intArrayFromString(id), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Get the group sequence for a message.
 * <p>
 * The group sequence of a message identifies the relative ordering of
 * messages within a group. The default value for the group sequence
 * of a message is zero.
 * @method getGroupSequence
 * @memberof! proton.Message#
 * @returns {number} the group sequence for the message.
 */
_Message_['getGroupSequence'] = function() {
    return _pn_message_get_group_sequence(this._message);
};

/**
 * Set the group sequence for a message. See {@link proton.Message.getGroupSequence}
 * for details on what the group sequence means.
 * @method setGroupSequence
 * @memberof! proton.Message#
 * @param {number} n the new group sequence for the message.
 */
_Message_['setGroupSequence'] = function(n) {
    this._check(_pn_message_set_group_sequence(this._message, n));
};

/**
 * Get the reply to group id for a message.
 * @method getReplyToGroupID
 * @memberof! proton.Message#
 * @returns {string} the reply to group id of the Message.
 */
_Message_['getReplyToGroupID'] = function() {
    return Pointer_stringify(_pn_message_get_reply_to_group_id(this._message));
};

/**
 * Set the reply to group id for a message.
 * @method setReplyToGroupID
 * @memberof! proton.Message#
 * @param {string} id the reply to group id we want to set for the Message.
 */
_Message_['setReplyToGroupID'] = function(id) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_reply_to_group_id(this._message, allocate(intArrayFromString(id), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * The following methods are marked as deprecated and are not implemented.
 * pn_message_get_format()
 * pn_message_set_format()
 * pn_message_load()
 * pn_message_load_data()
 * pn_message_load_text()
 * pn_message_load_amqp()
 * pn_message_load_json()
 * pn_message_save()
 * pn_message_save_data()
 * pn_message_save_text()
 * pn_message_save_amqp()
 * pn_message_save_json()
 * pn_message_data()
 */

/**
 * Return a Binary representation of the message encoded in AMQP format. N.B. the
 * returned {@link proton.Data.Binary} "owns" the underlying raw data and is thus
 * responsible for freeing it or passing it to a method that consumes a Binary
 * such as {@link proton.Message.decode}.
 * @method encode
 * @memberof! proton.Message#
 * @returns {proton.Data.Binary} a representation of the message encoded in AMQP format.
 */
_Message_['encode'] = function() {
    this._preEncode();
    var size = 1024;
    while (true) {
        setValue(size, size, 'i32'); // Set pass by reference variable.
        var bytes = _malloc(size);   // Allocate storage from emscripten heap.
        var err = _pn_message_encode(this._message, bytes, size);
        var size = getValue(size, 'i32'); // Dereference the real size value;

        if (err === Module['Error']['OVERFLOW']) {
            _free(bytes);
            size *= 2;
        } else if (err >= 0) {
            return new Data['Binary'](size, bytes);
        } else {
            _free(bytes);
            this._check(err);
            return;
        }
    }
};

/**
 * Decodes and loads the message content from supplied Binary AMQP data  N.B. 
 * this method "consumes" data from a {@link proton.Data.Binary} in other words
 * it takes responsibility for the underlying data and frees the raw data from
 * the Binary.
 * @method decode
 * @memberof! proton.Message#
 * @param {proton.Data.Binary} encoded the AMQP encoded binary message.
 */
_Message_['decode'] = function(encoded) {
    var err = _pn_message_decode(this._message, encoded.start, encoded.size);
    encoded['free'](); // Free the original Binary.
    if (err >= 0) {
        this._postDecode();
    }
    this._check(err);
};

