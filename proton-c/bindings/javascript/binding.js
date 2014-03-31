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

/**
 * This file provides a JavaScript wrapper around the Proton Messenger API.
 * It will be used to wrap the emscripten compiled proton-c code and minified by
 * the Closure compiler, so all comments will be stripped from the actual library.
 *
 * This JavaScript wrapper provides a slightly more object oriented interface which
 * abstracts the low-level emscripten based implementation details from client code.
 */
(function() { // Start of self-calling lambda used to avoid polluting global namespace.

/**
 * The Module Object is exported by emscripten for all execution platforms, we
 * use it as a namespace to allow us to selectively export only what we wish to
 * be publicly visible from this package/module. We will use the associative
 * array form for declaring exported properties to prevent the Closure compiler
 * from minifying e.g. Module['Messenger'] = ... to export the Messenger Object.
 * Exported Objects can be used in client code using the appropriate namespace:
 * proton = require("proton.js");
 * var messenger = new proton.Messenger();
 * var message = new proton.Message();
 */
var Module = {
    // Prevent emscripten runtime exiting, we will be enabling network callbacks.
    'noExitRuntime' : true
};



/**
 * This class is a buffer for use with the emscripten based port of zlib. It allows creation management of a 
 * buffer in the virtual heap space of the zlib library hiding the implementation detail from client code.
 */
/*
var Buffer = function(size) {
    var _public = {};
    var asize = 0; // The allocated size of the input buffer.
    var ptr   = 0; // Handle to the input buffer.

    // Private methods.
    function freeBuffer() {
        if (ptr !== 0) {
            _free(ptr);
        }
    };

    // Public methods
    _public.destroy = function() {
        freeBuffer();
    };

    _public.setSize = function(size) {
        if (size > asize) {
            freeBuffer();
            ptr = _malloc(size); // Get output buffer from emscripten.
            asize = size;
        }
        _public.size = size;
    };

    _public.getRaw = function() {
        return ptr;
    };

    _public.getBuffer = function() {
        // Get a Uint8Array view on the input buffer.
        return new Uint8Array(HEAPU8.buffer, ptr, _public.size);
    };

    if (size) {
        _public.setSize(size);
    }

    return _public;
};
*/


/*****************************************************************************/
/*                                                                           */
/*                                   Status                                  */
/*                                                                           */
/*****************************************************************************/

// Export Status Enum, avoiding minification.
Module['Status'] = {
    'UNKNOWN':  0, // PN_STATUS_UNKNOWN  The tracker is unknown.
    'PENDING':  1, // PN_STATUS_PENDING  The message is in flight.
                   //                    For outgoing messages, use messenger.isBuffered()
                   //                    to see if it has been sent or not.
    'ACCEPTED': 2, // PN_STATUS_ACCEPTED The message was accepted.
    'REJECTED': 3, // PN_STATUS_REJECTED The message was rejected.
    'RELEASED': 4, // PN_STATUS_RELEASED The message was released.
    'MODIFIED': 5, // PN_STATUS_MODIFIED The message was modified.
    'ABORTED':  6, // PN_STATUS_ABORTED  The message was aborted.
    'SETTLED':  7  // PN_STATUS_SETTLED  The remote party has settled the message.
};


/*****************************************************************************/
/*                                                                           */
/*                                   Error                                   */
/*                                                                           */
/*****************************************************************************/

// Export Error Enum, avoiding minification.
Module['Error'] = {
    'EOS':        -1, // PN_EOS
    'ERR':        -2, // PN_ERR
    'OVERFLOW':   -3, // PN_OVERFLOW
    'UNDERFLOW':  -4, // PN_UNDERFLOW
    'STATE_ERR':  -5, // PN_STATE_ERR
    'ARG_ERR':    -6, // PN_ARG_ERR
    'TIMEOUT':    -7, // PN_TIMEOUT
    'INTR':       -8, // PN_INTR
    'INPROGRESS': -9  // PN_INPROGRESS
};


/*****************************************************************************/
/*                                                                           */
/*                                 Messenger                                 */
/*                                                                           */
/*****************************************************************************/

Module['Messenger'] = function(name) { // Messenger Constructor.
    /**
     * The emscripten idiom below is used in a number of places in the JavaScript
     * bindings to map JavaScript Strings to C style strings. ALLOC_STACK will
     * increase the stack and place the item there. When the stack is next restored
     * (by calling Runtime.stackRestore()), that memory will be automatically
     * freed. In C code compiled by emscripten saving and restoring of the stack
     * is automatic, but if we want to us ALLOC_STACK from native JavaScript we
     * need to explicitly save and restore the stack using Runtime.stackSave()
     * and Runtime.stackRestore() or we will leak memory.
     * See https://github.com/kripken/emscripten/wiki/Interacting-with-code
     * The _pn_messenger constructor copies the char* passed to it.
     */
    var sp = Runtime.stackSave();
    this._messenger = _pn_messenger(name ? allocate(intArrayFromString(name), 'i8', ALLOC_STACK) : 0);
    Runtime.stackRestore(sp);

    /**
     * Initiate Messenger non-blocking mode. For JavaScript we make this the
     * default behaviour and don't export this method because JavaScript is
     * fundamentally an asynchronous non-blocking execution environment.
     */
    _pn_messenger_set_blocking(this._messenger, false);
};

Module['Messenger'].PN_CUMULATIVE = 0x1; // Protected Class attribute.

 // Expose prototype as a variable to make method declarations less verbose.
var _Messenger_ = Module['Messenger'].prototype;

// ************************* Protected methods ********************************

// We use the dot notation rather than associative array form for protected
// methods so they are visible to this "package", but the Closure compiler will
// minify and obfuscate names, effectively making a defacto "protected" method.

/**
 * This helper method checks the supplied error code, converts it into an
 * exception and throws the exception. This method will try to use the message
 * populated in pn_messenger_error(), if present, but if not it will fall
 * back to using the basic error code rendering from pn_code().
 * @param code the error code to check.
 */
_Messenger_._check = function(code) {
    if (code < 0) {
        if (code === Module['Error']['INPROGRESS']) {
            return code;
        }

        var errno = this['getErrno']();
        var message = errno ? this['getError']() : Pointer_stringify(_pn_code(code));

        throw { // TODO Improve name and level.
            name:     'Messenger Error', 
            level:    'Show Stopper', 
            message:  message, 
            toString: function() {return this.name + ': ' + this.message}
        };
    } else {
        return code;
    }
};

// *************************** Public methods *****************************

/**
 * N.B. The following methods are not exported by the JavaScript Messenger
 * binding for reasons described below.
 *
 * For these methods it is expected that security would be implemented via
 * a secure WebSocket. TODO what happens if we decide to implement TCP sockets
 * via Node.js net library. If we do that we may want to compile OpenSSL
 * using emscripten and include these methods.
 * pn_messenger_set_certificate()
 * pn_messenger_get_certificate()
 * pn_messenger_set_private_key()
 * pn_messenger_get_private_key()
 * pn_messenger_set_password()
 * pn_messenger_get_password()
 * pn_messenger_set_trusted_certificates()
 * pn_messenger_get_trusted_certificates()
 *
 * For these methods the implementation is fairly meaningless because JavaScript
 * is a fundamentally asynchronous non-blocking environment.
 * pn_messenger_set_timeout()
 * pn_messenger_set_blocking()
 * pn_messenger_interrupt()
 * pn_messenger_send() // Not sure if this is useful in JavaScript.
 */

/**
 * Retrieves the name of a Messenger.
 * @return the name of the messenger.
 */
_Messenger_['getName'] = function() {
    return Pointer_stringify(_pn_messenger_name(this._messenger));
};

/**
 * Retrieves the timeout for a Messenger.
 * @return zero because JavaScript is fundamentally non-blocking.
 */
_Messenger_['getTimeout'] = function() {
    return 0;
};

/**
 * Accessor for messenger blocking mode.
 * @return false because JavaScript is fundamentally non-blocking.
 */
_Messenger_['isBlocking'] = function() {
    return true;
};

/**
 * Free the Messenger. This will close all connections that are managed
 * by the Messenger. Call the stop method before destroying the Messenger.
 * N.B. This method has to be called explicitly in JavaScript as we can't
 * intercept finalisers, so we need to remember to free before removing refs.
 */
_Messenger_['free'] = function() {
    _pn_messenger_free(this._messenger);
};

/**
 * @return the most recent error message code.
 */
_Messenger_['getErrno'] = function() {
    return _pn_messenger_errno(this._messenger);
};

/**
 * @return the most recent error message as a String.
 */
_Messenger_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_messenger_error(this._messenger)));
};

/**
 * Returns the size of the outgoing window that was set with
 * pn_messenger_set_outgoing_window. The default is 0.
 * @return the outgoing window.
 */
_Messenger_['getOutgoingWindow'] = function() {
    return _pn_messenger_get_outgoing_window(this._messenger);
};

/**
 * Sets the outgoing tracking window for the Messenger. The Messenger will
 * track the remote status of this many outgoing deliveries after calling
 * send. Defaults to zero.
 *
 * A Message enters this window when you call put() with the Message.
 * If your outgoing window size is n, and you call put() n+1 times, status
 * information will no longer be available for the first Message.
 * @param window the size of the tracking window in messages.
 */
_Messenger_['setOutgoingWindow'] = function(window) {
    this._check(_pn_messenger_set_outgoing_window(this._messenger, window));
};

/**
 * Returns the size of the incoming window that was set with
 * pn_messenger_set_incoming_window. The default is 0.
 * @return the incoming window.
 */
_Messenger_['getIncomingWindow'] = function() {
    return _pn_messenger_get_incoming_window(this._messenger);
};

/**
 * Sets the incoming tracking window for the Messenger. The Messenger will
 * track the remote status of this many incoming deliveries after calling
 * send. Defaults to zero.
 *
 * Messages enter this window only when you take them into your application
 * using get(). If your incoming window size is n, and you get() n+1 messages
 * without explicitly accepting or rejecting the oldest message, then the
 * Message that passes beyond the edge of the incoming window will be assigned
 * the default disposition of its link.
 * @param window the size of the tracking window in messages.
 */
_Messenger_['setIncomingWindow'] = function(window) {
    this._check(_pn_messenger_set_incoming_window(this._messenger, window));
};

/**
 * Currently a no-op placeholder. For future compatibility, do not send or
 * recv messages before starting the Messenger.
 */
_Messenger_['start'] = function() {
    this._check(_pn_messenger_start(this._messenger));
};

/**
 * Transitions the Messenger to an inactive state. An inactive Messenger
 * will not send or receive messages from its internal queues. A Messenger
 * should be stopped before being discarded to ensure a clean shutdown
 * handshake occurs on any internally managed connections.
 *
 * The Messenger may require some time to stop if it is busy, and in that
 * case will return PN_INPROGRESS. In that case, call isStopped to see if
 * it has fully stopped.
 */
_Messenger_['stop'] = function() {
    return this._check(_pn_messenger_stop(this._messenger));
};

/**
 * @return Returns true iff a Messenger is in the stopped state.
 */
_Messenger_['isStopped'] = function() {
    return (_pn_messenger_stopped(this._messenger) > 0);
};

/**
 * Subscribes the Messenger to messages originating from the
 * specified source. The source is an address as specified in the
 * Messenger introduction with the following addition. If the
 * domain portion of the address begins with the '~' character, the
 * Messenger will interpret the domain as host/port, bind to it,
 * and listen for incoming messages. For example "~0.0.0.0",
 * "amqp://~0.0.0.0", and "amqps://~0.0.0.0" will all bind to any
 * local interface and listen for incoming messages with the last
 * variant only permitting incoming SSL connections.
 * @param source the source address we're subscribing to.
 * @return a subscription.
 */
_Messenger_['subscribe'] = function(source) {
    if (!source) {
        this._check(Module['Error']['ARG_ERR']);
    }
    var sp = Runtime.stackSave();
    var subscription = _pn_messenger_subscribe(this._messenger,
                                               allocate(intArrayFromString(source), 'i8', ALLOC_STACK));
    Runtime.stackRestore(sp);
    if (!subscription) {
        this._check(Module['Error']['ERR']);
    }
    return new Subscription(subscription);
};

/**
 * Places the content contained in the message onto the outgoing queue
 * of the Messenger. This method will never block, however it will send any
 * unblocked Messages in the outgoing queue immediately and leave any blocked
 * Messages remaining in the outgoing queue. The send call may be used to
 * block until the outgoing queue is empty. The outgoing property may be
 * used to check the depth of the outgoing queue.
 *
 * When the content in a given Message object is copied to the outgoing
 * message queue, you may then modify or discard the Message object
 * without having any impact on the content in the outgoing queue.
 *
 * This method returns an outgoing tracker for the Message.  The tracker
 * can be used to determine the delivery status of the Message.
 * @param message a Message to send.
 * @return a tracker.
 */
_Messenger_['put'] = function(message) {
    message._preEncode();
    this._check(_pn_messenger_put(this._messenger, message._message));

    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var low = _pn_messenger_outgoing_tracker(this._messenger);
    var high = tempRet0;
    return new Data.Long(low, high);
};

/**
 * Gets the last known remote state of the delivery associated with the given tracker.
 * @param tracker the tracker whose status is to be retrieved.
 * @return one of None, PENDING, REJECTED, or ACCEPTED.
 */
_Messenger_['status'] = function(tracker) {
    return _pn_messenger_status(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits());
};

/**
 * Checks if the delivery associated with the given tracker is still waiting to be sent.
 * @param tracker the tracker identifying the delivery.
 * @return true if delivery is still buffered.
 */
_Messenger_['isBuffered'] = function(tracker) {
    return (_pn_messenger_buffered(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits()) > 0);
};

/**
 * Frees a Messenger from tracking the status associated with a given tracker.
 * If you don't supply a tracker, all outgoing messages up to the most recent
 * will be settled.
 * @param tracker the tracker identifying the delivery.
 */
_Messenger_['settle'] = function(tracker) {
console.log("settle: not fully tested yet");
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_outgoing_tracker(this._messenger);
        var high = tempRet0;
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_settle(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Sends or receives any outstanding messages queued for a Messenger.
 * For JavaScript the only timeout that makes sense is 0 == do not block.
 * This method may also do I/O work other than sending and receiving messages.
 * For example, closing connections after messenger.stop() has been called.
 * @return 0 if no work to do, < 0 if error, or 1 if work was done.
 */
_Messenger_['work'] = function() {
    return _pn_messenger_work(this._messenger, 0);
};

/**
 * Receives up to limit messages into the incoming queue.  If no value for limit
 * is supplied, this call will receive as many messages as it can buffer internally.
 * @param limit the maximum number of messages to receive or -1 to to receive
 *        as many messages as it can buffer internally.
 */
_Messenger_['recv'] = function(limit) {
    this._check(_pn_messenger_recv(this._messenger, (limit ? limit : -1)));
};

/**
 * Returns the capacity of the incoming message queue of messenger. Note this
 * count does not include those messages already available on the incoming queue.
 * @return the message queue capacity.
 */
_Messenger_['receiving'] = function() {
    return _pn_messenger_receiving(this._messenger);
};

/**
 * Moves the message from the head of the incoming message queue into the
 * supplied message object. Any content in the message will be overwritten.
 *
 * A tracker for the incoming Message is returned. The tracker can later be
 * used to communicate your acceptance or rejection of the Message.
 *
 * @param message the destination message object. If no Message object is
 *        passed, the Message popped from the head of the queue is discarded.
 * @return a tracker for the incoming Message.
 */
_Messenger_['get'] = function(message) {
    var impl = null;
    if (message) {
        impl = message._message;
    }

    this._check(_pn_messenger_get(this._messenger, impl));

    if (message) {
        message._postDecode();
    }

    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var low = _pn_messenger_incoming_tracker(this._messenger);
    var high = tempRet0;
console.log("get low = " + low);
console.log("get high = " + high);
    return new Data.Long(low, high);
};

/**
 * Returns the Subscription of the Message returned by the most recent call
 * to get, or null if pn_messenger_get has not yet been called.
 * @return a Subscription or null if get has never been called for this Messenger.
 */
_Messenger_['incomingSubscription'] = function() {row
console.log("incomingSubscription: haven't yet proved this works yet");

    var subscription = _pn_messenger_incoming_subscription(this._messenger);
    if (subscription) {
        return new Subscription(subscription);
    } else {
        return null;
    }
};

/**
 * Signal the sender that you have acted on the Message pointed to by the tracker.
 * If no tracker is supplied, then all messages that have been returned by the
 * get method are accepted, except those that have already been auto-settled
 * by passing beyond your incoming window size.
 * @param tracker the tracker identifying the delivery.
 */
_Messenger_['accept'] = function(tracker) {
console.log("accept: not fully tested yet");
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_incoming_tracker(this._messenger);
        var high = tempRet0;
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_accept(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Rejects the Message indicated by the tracker.  If no tracker is supplied,
 * all messages that have been returned by the get method are rejected, except
 * those already auto-settled by passing beyond your outgoing window size.
 * @param tracker the tracker identifying the delivery.
 */
_Messenger_['reject'] = function(tracker) {
console.log("reject: not fully tested yet");
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_incoming_tracker(this._messenger);
        var high = tempRet0;
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_reject(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Returns the number of messages in the outgoing message queue of a messenger.
 * @return the outgoing queue depth.
 */
_Messenger_['outgoing'] = function() {
    return _pn_messenger_outgoing(this._messenger);
};

/**
 * Returns the number of messages in the incoming message queue of a messenger.
 * @return the incoming queue depth.
 */
_Messenger_['incoming'] = function() {
    return _pn_messenger_incoming(this._messenger);
};



/**
 * 
 * @param pattern a glob pattern to select messages.
 * @param address an address indicating outgoing address rewrite.
 */
_Messenger_['route'] = function(pattern, address) {
console.log("route: not fully tested yet");
    var sp = Runtime.stackSave();
    this._check(_pn_messenger_route(this._messenger,
                                    allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                    allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Similar to route(), except that the destination of the Message is determined
 * before the message address is rewritten.
 *
 * The outgoing address is only rewritten after routing has been finalized. If
 * a message has an outgoing address of "amqp://0.0.0.0:5678", and a rewriting
 * rule that changes its outgoing address to "foo", it will still arrive at the
 * peer that is listening on "amqp://0.0.0.0:5678", but when it arrives there,
 * the receiver will see its outgoing address as "foo".
 *
 * The default rewrite rule removes username and password from addresses
 * before they are transmitted.
 * @param pattern a glob pattern to select messages.
 * @param address an address indicating outgoing address rewrite.
 */
_Messenger_['rewrite'] = function(pattern, address) {
console.log("rewrite: not fully tested yet");
    var sp = Runtime.stackSave();
    this._check(_pn_messenger_rewrite(this._messenger,
                                      allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                      allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};




// TODO This needs tweaking to enable working with multiple Messenger instances.
_Messenger_['setNetworkCallback'] = function(callback) {
//console.log("setting network callback");

    // Expose messenger reference in the scope of Messenger.Messenger so that
    // the _work function can correctly dereference it.
    var messenger = this._messenger;

    function _work() {
        //console.log("                          *** internal work ***");

        var err = _pn_messenger_work(messenger, 0);
//console.log("err = " + err);

        if (err >= 0) {
            callback();
        }

        err = _pn_messenger_work(messenger, 0);
//console.log("err = " + err);

        if (err >= 0) {
            callback();
        }
    };

    // Set the emscripten network callback function.
    Module['networkCallback'] = _work;
};






/*****************************************************************************/
/*                                                                           */
/*                               Subscription                                */
/*                                                                           */
/*****************************************************************************/

/**
 * This class is a wrapper for Messenger's subscriptions.
 * Subscriptions should never be *directly* instantiated by client code only via
 * Messenger.subscribe() or Messenger.incomingSubscription(), so we declare the
 * constructor in the scope of the package and don't export it via Module.
 */
var Subscription = function(subscription) { // Subscription Constructor.
    this._subscription = subscription;
};

/**
 * TODO Not sure exactly what pn_subscription_get_context does.
 * @return the Subscription's Context.
 */
Subscription.prototype['getContext'] = function() {
    return _pn_subscription_get_context(this._subscription);
};

/**
 * TODO Not sure exactly what pn_subscription_set_context does.
 * @param context the Subscription's new Context.
 */
Subscription.prototype['setContext'] = function(context) {
    _pn_subscription_set_context(this._subscription, context);
};

/**
 * @return the Subscription's Address.
 */
Subscription.prototype['getAddress'] = function() {
    return Pointer_stringify(_pn_subscription_address(this._subscription));
};




/*****************************************************************************/
/*                                                                           */
/*                                  Message                                  */
/*                                                                           */
/*****************************************************************************/

Module['Message'] = function() { // Message Constructor.
    this._message = _pn_message();

    // ************************* Public properties ****************************

    /**
     * Delivery instructions for the Message.
     * @type map
     */
    this['instructions'] = null;

    /**
     * Infrastructure defined Message annotations.
     * @type map
     */
    this['annotations'] = null;

    /**
     * Application defined Message properties.
     * @type map
     */
    this['properties'] = null;

    /**
     * Message body.
     * @type bytes | unicode | map | list | int | long | float | UUID
     */
    this['body'] = null;
};

// Expose prototype as a variable to make method declarations less verbose.
var _Message_ = Module['Message'].prototype;

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

        throw { // TODO Improve name and level.
            name:     'Message Error', 
            level:    'Show Stopper', 
            message:  message, 
            toString: function() {return this.name + ': ' + this.message} 
        };
    } else {
        return code;
    }
};

/**
 * Encode the Message prior to sending on the wire.
 */
_Message_._preEncode = function() {
    if (this.instructions) {
console.log("Encoding instructions");
        var inst = new Data(_pn_message_instructions(this._message));
        inst.clear();
        inst.putObject(this.instructions);
    }

    if (this.annotations) {
console.log("Encoding annotations");
        var ann = new Data(_pn_message_annotations(this._message));
        ann.clear();
        ann.putObject(this.annotations);
    }

    if (this.properties) {
console.log("Encoding properties");
        var props = new Data(_pn_message_properties(this._message));
        props.clear();
        props.putObject(this.properties);
    }

    if (this.body != null) {
console.log("Encoding body");
        var body = new Data(_pn_message_body(this._message));
        body.clear();
        body.putObject(this.body);
    }
};

/**
 * Decode the Message after receiving off the wire.
 */
_Message_._postDecode = function() {
    var inst = new Data(_pn_message_instructions(this._message));
    var ann = new Data(_pn_message_annotations(this._message));
    var props = new Data(_pn_message_properties(this._message));
    var body = new Data(_pn_message_body(this._message));

    if (inst.next()) {
        this.instructions = inst.getObject();
    } else {
        this.instructions = {};
    }

    if (ann.next()) {
        this.annotations = ann.getObject();
    } else {
        this.annotations = {};
    }

    if (props.next()) {
        this.properties = props.getObject();
    } else {
        this.properties = {};
    }

    if (body.next()) {
        this.body = body.getObject();
    } else {
        this.body = null;
    }
};

// *************************** Public methods *********************************

/**
 * Free the Message.
 * N.B. This method has to be called explicitly in JavaScript as we can't
 * intercept finalisers, so we need to remember to free before removing refs.
 */
_Message_['free'] = function() {
    _pn_message_free(this._message);
};

/**
 * @return the most recent error message code.
 */
_Message_['getErrno'] = function() {
    return _pn_message_errno(this._message);
};

/**
 * @return the most recent error message as a String.
 */
_Message_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_message_error(this._message)));
};

/**
 * @return the address of the Message.
 */
_Message_['getAddress'] = function() {
    return Pointer_stringify(_pn_message_get_address(this._message));
};

/**
 * Set the address of the Message.
 * @param address the address we want to send the Message to.
 */
_Message_['setAddress'] = function(address) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_address(this._message, allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * @return the subject of the Message.
 */
_Message_['getSubject'] = function() {
    return Pointer_stringify(_pn_message_get_subject(this._message));
};

/**
 * Set the subject of the Message.
 * @param subject the subject we want to set for the Message.
 */
_Message_['setSubject'] = function(subject) {
    var sp = Runtime.stackSave();
    this._check(_pn_message_set_subject(this._message, allocate(intArrayFromString(subject), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};














/*****************************************************************************/
/*                                                                           */
/*                                    Data                                   */
/*                                                                           */
/*****************************************************************************/

/**
 * The Data class provides an interface for decoding, extracting, creating, and
 * encoding arbitrary AMQP data. A Data object contains a tree of AMQP values.
 * Leaf nodes in this tree correspond to scalars in the AMQP type system such as
 * ints<INT> or strings<STRING>. Non-leaf nodes in this tree correspond to compound
 * values in the AMQP type system such as lists<LIST>, maps<MAP>, arrays<ARRAY>,
 * or described values<DESCRIBED>. The root node of the tree is the Data object
 * itself and can have an arbitrary number of children.
 *
 * A Data object maintains the notion of the current sibling node and a current
 * parent node. Siblings are ordered within their parent. Values are accessed
 * and/or added by using the next, prev, enter, and exit methods to navigate to
 * the desired location in the tree and using the supplied variety of put* and
 * get* methods to access or add a value of the desired type.
 *
 * The put* methods will always add a value after the current node in the tree.
 * If the current node has a next sibling the put* method will overwrite the value
 * on this node. If there is no current node or the current node has no next
 * sibling then one will be added. The put* methods always set the added/modified
 * node to the current node. The get* methods read the value of the current node
 * and do not change which node is current.                                                                         
 */
Module['Data'] = function(data) { // Data Constructor.
    this._data = data;

    if (!data) {
        console.log("Warning Data created with null data");
    }
};

// Expose constructor as package scope variable to make internal calls less verbose.
var Data = Module['Data'];

// Expose prototype as a variable to make method declarations less verbose.
var _Data_ = Data.prototype;

/**
 * Look-up table mapping proton-c types to the accessor method used to
 * deserialise the type. N.B. this is a simple Array and not a map because the
 * types that we get back from pn_data_type are integers from the pn_type_t enum.
 */
Data.GetMappings = [
    'getNull',            // 0
    'getNull',            // PN_NULL = 1
    'getBoolean',         // PN_BOOL = 2
    'getUnsignedByte',    // PN_UBYTE = 3
    'getByte',            // PN_BYTE = 4
    'getUnsignedShort',   // PN_USHORT = 5
    'getShort',           // PN_SHORT = 6
    'getUnsignedInteger', // PN_UINT = 7
    'getInteger',         // PN_INT = 8
    'getChar',            // PN_CHAR = 9
    'getUnsignedLong',    // PN_ULONG = 10
    'getLong',            // PN_LONG = 11
    'getTimestamp',       // PN_TIMESTAMP = 12
    'getFloat',           // PN_FLOAT = 13
    'getDouble',          // PN_DOUBLE = 14
    'getDecimal32',       // PN_DECIMAL32 = 15
    'getDecimal64',       // PN_DECIMAL64 = 16
    'getDecimal128',      // PN_DECIMAL128 = 17
    'getUUID',            // PN_UUID = 18
    'getBinary',          // PN_BINARY = 19
    'getString',          // PN_STRING = 20
    'getSymbol',          // PN_SYMBOL = 21
    'getDescribed',       // PN_DESCRIBED = 22
    'getArray',           // PN_ARRAY = 23
    'getJSArray',         // PN_LIST = 24
    'getDictionary'       // PN_MAP = 25
];

// *************************** Class methods **********************************

/**
 * Test if a given Object is an Array.
 * @param o the Object that we wish to test.
 * @return true iff the Object is an Array.
 */
Data.isArray = Array.isArray || function(o) {
console.log("Data.isArray");
    return Object.prototype.toString.call(o) === '[object Array]';
};

/**
 * Test if a given Object is a Number.
 * @param o the Object that we wish to test.
 * @return true iff the Object is a Number.
 */
Data.isNumber = function(o) {
    return typeof o === 'number' || 
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object Number]');
};

/**
 * Test if a given Object is a String.
 * @param o the Object that we wish to test.
 * @return true iff the Object is a String.
 */
Data.isString = function(o) {
    return typeof o === 'string' ||
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object String]');
};

/**
 * Test if a given Object is a Boolean.
 * @param o the Object that we wish to test.
 * @return true iff the Object is a Boolean.
 */
Data.isBoolean = function(o) {
    return typeof o === 'boolean' ||
          (typeof o === 'object' && Object.prototype.toString.call(o) === '[object Boolean]');
};

// **************************** Inner Classes *********************************

// --------------------------- proton.Data.UUID -------------------------------
/**
 * Create a proton.Data.UUID which is a type 4 UUID.
 * @param u a UUID. If null a type 4 UUID is generated wich may use crypto if
 *        supported by the platform. If u is an emscripten "pointer" we copy the
 *        data from that. If u is a JavaScript Array we use it as-is. If u is a
 *        String then we try to parse that as a UUID.
 * @properties uuid is the compact array form of the UUID.
 */
Data['UUID'] = function(u) { // Data.UUID Constructor.
    // Helper to copy from emscriptem allocated storage into JavaScript Array.
    function _p2a(p) {
        var uuid = new Array(16);
        for (var i = 0; i < 16; i++) {
            uuid[i] = getValue(p + i, 'i8') & 0xFF; // & 0xFF converts to unsigned.
        }
        return uuid;
    };

    if (!u) { // Generate UUID using emscriptem's uuid_generate implementation.
        var sp = Runtime.stackSave();
        var p = allocate(16, 'i8', ALLOC_STACK); // Create temporary pointer storage.
        _uuid_generate(p);      // Generate UUID into allocated pointer.
        this['uuid'] = _p2a(p); // Copy from allocated storage into JavaScript Array.
        Runtime.stackRestore(sp);
    } else if (Data.isNumber(u)) { // Use pointer that has been passed in.
        this['uuid'] = _p2a(u);    // Copy from allocated storage into JavaScript Array.
    } else if (Data.isArray(u)) { // Use array that has been passed in.
        this['uuid'] = u; // Just use the JavaScript Array.
    } else if (Data.isString(u)) { // Parse String form UUID.
        if (u.length === 36) {
            var i = 0;
            var uuid = new Array(16);
            u.toLowerCase().replace(/[0-9a-f]{2}/g, function(byte) {
                if (i < 16) {
                    uuid[i++] = parseInt(byte, 16);
                }
            });
            this['uuid'] = uuid;
        }
    }
    this.string = null;
};

/**
 * @return the String form of a proton.Data.UUID.
 */
Data['UUID'].prototype.toString = function() {
    if (!this.string) { // Check if we've cached the string version.
        var i = 0;
        var uu = this['uuid'];
        var uuid = 'xxxx-xx-xx-xx-xxxxxx'.replace(/[x]/g, function(c) {
            var r = uu[i].toString(16);
            r = (r.length === 1) ? '0' + r : r; // Zero pad single digit hex values
            i++;
            return r;
        });
        this.string = uuid;
    }
    return this.string;
};

/**
 * Compare two instances of proton.Data.UUID for equality.
 * @param rhs the instance we wish to compare this instance with.
 * @return true iff the two compared instances are equal.
 */
Data['UUID'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};


// ---------------------------- proton.Data.Symbol ---------------------------- 
/**
 * Create a proton.Data.Symbol.
 * @param s a symbol
 */
Data['Symbol'] = function(s) { // Data.Symbol Constructor.
    this.value = s;
    this.length = this.value.length;
};

/**
 * @return the String form of a proton.Data.Symbol.
 */
Data['Symbol'].prototype.toString = Data['Symbol'].prototype.valueOf = function() {
    return this.value;
};

/**
 * Compare two instances of proton.Data.Symbol for equality.
 * @param rhs the instance we wish to compare this instance with.
 * @return true iff the two compared instances are equal.
 */
Data['Symbol'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};


// ----------------------------- proton.Data.Long ----------------------------- 
/**

 */
Data.Long = function(low, high) { // Data.Long Constructor.
    this.low  = low  | 0;  // force into 32 signed bits.
    this.high = high | 0;  // force into 32 signed bits.
};

// proton.Data.Long constants.
Data.Long.TWO_PWR_16_DBL_ = 1 << 16;
Data.Long.TWO_PWR_32_DBL_ = Data.Long.TWO_PWR_16_DBL_ * Data.Long.TWO_PWR_16_DBL_;
Data.Long.TWO_PWR_64_DBL_ = Data.Long.TWO_PWR_32_DBL_ * Data.Long.TWO_PWR_32_DBL_;
Data.Long.TWO_PWR_63_DBL_ = Data.Long.TWO_PWR_64_DBL_ / 2;
Data.Long.MAX_VALUE = new Data.Long(0xFFFFFFFF | 0, 0x7FFFFFFF | 0);
Data.Long.MIN_VALUE = new Data.Long(0, 0x80000000 | 0);
Data.Long.ZERO = new Data.Long(0, 0);
Data.Long.ONE  = new Data.Long(1, 0);

Data.Long.fromNumber = function(value) {
    if (isNaN(value) || !isFinite(value)) {
        return Data.Long.ZERO;
    } else if (value <= -Data.Long.TWO_PWR_63_DBL_) {
        return Data.MIN_VALUE;
    } else if (value + 1 >= Data.Long.TWO_PWR_63_DBL_) {
        return Data.Long.MAX_VALUE;
    } else if (value < 0) {
        return Data.Long.fromNumber(-value).negate();
    } else {
      return new Data.Long(
          (value % Data.Long.TWO_PWR_32_DBL_) | 0,
          (value / Data.Long.TWO_PWR_32_DBL_) | 0);
    }
};

Data.Long.prototype.negate = function() {
    if (this.equals(Data.Long.MIN_VALUE)) {
        return Data.Long.MIN_VALUE;
    } else {
        return this.not().add(Data.Long.ONE);
    }
};

Data.Long.prototype.add = function(other) {
    // Divide each number into 4 chunks of 16 bits, and then sum the chunks.

    var a48 = this.high >>> 16;
    var a32 = this.high & 0xFFFF;
    var a16 = this.low >>> 16;
    var a00 = this.low & 0xFFFF;

    var b48 = other.high >>> 16;
    var b32 = other.high & 0xFFFF;
    var b16 = other.low >>> 16;
    var b00 = other.low & 0xFFFF;

    var c48 = 0, c32 = 0, c16 = 0, c00 = 0;
    c00 += a00 + b00;
    c16 += c00 >>> 16;
    c00 &= 0xFFFF;
    c16 += a16 + b16;
    c32 += c16 >>> 16;
    c16 &= 0xFFFF;
    c32 += a32 + b32;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c48 += a48 + b48;
    c48 &= 0xFFFF;
    return new Data.Long((c16 << 16) | c00, (c48 << 16) | c32);
};

Data.Long.prototype.not = function() {
    return new Data.Long(~this.low, ~this.high);
};

Data.Long.prototype.equals = function(other) {
    return (this.high == other.high) && (this.low == other.low);
};

Data.Long.prototype.getHighBits = function() {
    return this.high;
};

Data.Long.prototype.getLowBits = function() {
    return this.low;
};

Data.Long.prototype.getLowBitsUnsigned = function() {
    return (this.low >= 0) ? this.low : Data.Long.TWO_PWR_32_DBL_ + this.low;
};

Data.Long.prototype.toNumber = function() {
    return (this.high * Data.Long.TWO_PWR_32_DBL_) + this.getLowBitsUnsigned();
};

Data.Long.prototype.toString = function() {
    return this.high + ":" + this.getLowBitsUnsigned();
};

// ---------------------------- proton.Data.Binary ---------------------------- 


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

        throw { // TODO Improve name and level.
            name:     'Data Error', 
            level:    'Show Stopper', 
            message:  message, 
            toString: function() {return this.name + ': ' + this.message} 
        };
    } else {
        return code;
    }
};


// *************************** Public methods *********************************

/**
 * @return the most recent error message code.
 */
_Data_['getErrno'] = function() {
    return _pn_data_errno(this._data);
};

/**
 * @return the most recent error message as a String.
 */
_Data_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_data_error(this._data)));
};

/**
 * Clears the data object.
 */
_Data_['clear'] = function() {
    _pn_data_clear(this._data);
};

/**
 * Clears current node and sets the parent to the root node.  Clearing the
 * current node sets it _before_ the first node, calling next() will advance
 * to the first node.
 */
_Data_['rewind'] = function() {
    _pn_data_rewind(this._data);
};

/**
 * Advances the current node to its next sibling and returns its
 * type. If there is no next sibling the current node remains
 * unchanged and null is returned.
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
 * Advances the current node to its previous sibling and returns its
 * type. If there is no previous sibling the current node remains
 * unchanged and null is returned.
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
 * Sets the parent node to the current node and clears the current node.
 * Clearing the current node sets it _before_ the first child,
 * call next() advances to the first child.
 */
_Data_['enter'] = function() {
    return (_pn_data_enter(this._data) > 0);
};

/**
 * Sets the current node to the parent node and the parent node to
 * its own parent.
 */
_Data_['exit'] = function() {
    return (_pn_data_exit(this._data) > 0);
};



_Data_['lookup'] = function(name) {
    var sp = Runtime.stackSave();
    var lookup = _pn_data_lookup(this._data, allocate(intArrayFromString(name), 'i8', ALLOC_STACK));
    Runtime.stackRestore(sp);
    return (lookup > 0);
};

_Data_['narrow'] = function() {
    _pn_data_narrow(this._data);
};

_Data_['widen'] = function() {
    _pn_data_widen(this._data);
};


/**
 * @return the type of the current node.
 */
_Data_['type'] = function() {
    var dtype = _pn_data_type(this._data);
    if (dtype === -1) {
        return null;
    } else {
        return dtype;
    }
};

// TODO encode and decode

/**
 * Puts a list value. Elements may be filled by entering the list
 * node and putting element values.
 *
 *  data = new Data();
 *  data.putList();
 *  data.enter();
 *  data.putInt(1);
 *  data.putInt(2);
 *  data.putInt(3);
 *  data.exit();
 */
_Data_['putList'] = function() {
    this._check(_pn_data_put_list(this._data));
};

/**
 * Puts a map value. Elements may be filled by entering the map node
 * and putting alternating key value pairs.
 *
 *  data = new Data();
 *  data.putMap();
 *  data.enter();
 *  data.putString("key");
 *  data.putString("value");
 *  data.exit();
 */
_Data_['putMap'] = function() {
    this._check(_pn_data_put_map(this._data));
};

// TODO putArray and putDescribed

/**
 * Puts a null value.
 */
_Data_['putNull'] = function() {
    this._check(_pn_data_put_null(this._data));
};

/**
 * Puts a boolean value.
 * @param b a boolean value.
 */
_Data_['putBoolean'] = function(b) {
    this._check(_pn_data_put_bool(this._data, b));
};

/**
 * Puts a unsigned byte value.
 * @param ub an integral value.
 */
_Data_['putUnsignedByte'] = function(ub) {
    this._check(_pn_data_put_ubyte(this._data, ub));
};

/**
 * Puts a signed byte value.
 * @param b an integral value.
 */
_Data_['putByte'] = function(b) {
    this._check(_pn_data_put_byte(this._data, b));
};

/**
 * Puts a unsigned short value.
 * @param us an integral value.
 */
_Data_['putUnsignedShort'] = function(us) {
    this._check(_pn_data_put_ushort(this._data, us));
};

/**
 * Puts a signed short value.
 * @param s an integral value.
 */
_Data_['putShort'] = function(s) {
    this._check(_pn_data_put_short(this._data, s));
};

/**
 * Puts a unsigned integer value.
 * @param ui an integral value.
 */
_Data_['putUnsignedInteger'] = function(ui) {
    this._check(_pn_data_put_uint(this._data, ui));
};

/**
 * Puts a signed integer value.
 * @param i an integral value.
 */
_Data_['putInteger'] = function(i) {
    this._check(_pn_data_put_int(this._data, i));
};

/**
 * Puts a signed char value.
 * @param c a single character.
 */
_Data_['putChar'] = function(c) {
console.log("putChar not properly implemented yet");
    this._check(_pn_data_put_char(this._data, c));
};

/**
 * Puts a unsigned long value.
 * @param ul an integral value.
 */
_Data_['putUnsignedLong'] = function(ul) {
    this._check(_pn_data_put_ulong(this._data, ul));
};

/**
 * Puts a signed long value.
 * @param i an integral value.
 */
_Data_['putLong'] = function(l) {
console.log("putLong " + l);

    var long = Data.Long.fromNumber(l);
    this._check(_pn_data_put_long(this._data, long.getLowBitsUnsigned(), long.getHighBits()));
};

/**
 * Puts a timestamp.
 * @param t an integral value.
 */
_Data_['putTimestamp'] = function(t) {
console.log("putTimestamp not properly implemented yet");
    this._check(_pn_data_put_timestamp(this._data, t));
};

/**
 * Puts a float value.
 * @param f a floating point value.
 */
_Data_['putFloat'] = function(f) {
    this._check(_pn_data_put_float(this._data, f));
};

/**
 * Puts a double value.
 * @param d a floating point value.
 */
_Data_['putDouble'] = function(d) {
    this._check(_pn_data_put_double(this._data, d));
};

/**
 * Puts a decimal32 value.
 * @param d a decimal32 value.
 */
_Data_['putDecimal32'] = function(d) {
    this._check(_pn_data_put_decimal32(this._data, d));
};

/**
 * Puts a decimal64 value.
 * @param d a decimal64 value.
 */
_Data_['putDecimal64'] = function(d) {
    this._check(_pn_data_put_decimal64(this._data, d));
};

/**
 * Puts a decimal128 value.
 * @param d a decimal128 value.
 */
_Data_['putDecimal128'] = function(d) {
    this._check(_pn_data_put_decimal128(this._data, d));
};

/**
 * Puts a UUID value.
 * @param u a uuid value
 */
_Data_['putUUID'] = function(u) {
    var sp = Runtime.stackSave();
    this._check(_pn_data_put_uuid(this._data, allocate(u['uuid'], 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Puts a binary value.
 * @param b a binary value.
 */
_Data_['putBinary'] = function(d) {
console.log("putBinary not properly implemented yet");
    this._check(_pn_data_put_binary(this._data, b));
};

/**
 * Puts a unicode string value.
 * @param s a unicode string value.
 */
_Data_['putString'] = function(s) {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_data_put_string(data, pn_bytes(strlen(text), text));

    // First create an array from the JavaScript String using the intArrayFromString
    // helper function (from emscripten/src/preamble.js). We use this idiom in a
    // few places but here we create array as a separate var as we need its length.
    var array = intArrayFromString(s);
    // Allocate temporary storage for the array on the emscripten stack.
    var str = allocate(array, 'i8', ALLOC_STACK);

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the compiled
    // pn_bytes. The second parameter is the length of the array - 1 because it
    // is a NULL terminated C style string at this point.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, array.length - 1, str);

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
 * @param s the symbol name.
 */
_Data_['putSymbol'] = function(s) {
    var sp = Runtime.stackSave();
    // The implementation here is a bit "quirky" due to some low-level details
    // of the interaction between emscripten and LLVM and the use of pn_bytes.
    // The JavaScript code below is basically a binding to:
    //
    // pn_data_put_symbol(data, pn_bytes(strlen(text), text));

    // First create an array from the JavaScript String using the intArrayFromString
    // helper function (from emscripten/src/preamble.js). We use this idiom in a
    // few places but here we create array as a separate var as we need its length.
    var array = intArrayFromString(s);
    // Allocate temporary storage for the array on the emscripten stack.
    var str = allocate(array, 'i8', ALLOC_STACK);

    // Here's the quirky bit, pn_bytes actually returns pn_bytes_t *by value* but
    // the low-level code handles this *by pointer* so we first need to allocate
    // 8 bytes storage for {size, start} on the emscripten stack and then we
    // pass the pointer to that storage as the first parameter to the compiled
    // pn_bytes. The second parameter is the length of the array - 1 because it
    // is a NULL terminated C style string at this point.
    var bytes = allocate(8, 'i8', ALLOC_STACK);
    _pn_bytes(bytes, array.length - 1, str);

    // The compiled pn_data_put_symbol takes the pn_bytes_t by reference not value.
    this._check(_pn_data_put_symbol(this._data, bytes));
    Runtime.stackRestore(sp);
};






























// TODO getArray and isDescribed

/**
 * @return a null value.
 */
_Data_['getNull'] = function() {
    return null;
};

/**
 * Checks if the current node is a null.
 */
_Data_['isNull'] = function() {
    return _pn_data_is_null(this._data);
};

/**
 * @return value if the current node is a boolean, returns false otherwise.
 */
_Data_['getBoolean'] = function() {
    return (_pn_data_get_bool(this._data) > 0);
};

/**
 * @return value if the current node is an unsigned byte, returns 0 otherwise.
 */
_Data_['getUnsignedByte'] = function() {
    return _pn_data_get_ubyte(this._data);
};

/**
 * @return value if the current node is a signed byte, returns 0 otherwise.
 */
_Data_['getByte'] = function() {
    return _pn_data_get_byte(this._data);
};

/**
 * @return value if the current node is an unsigned short, returns 0 otherwise.
 */
_Data_['getUnsignedShort'] = function() {
    return _pn_data_get_ushort(this._data);
};

/**
 * @return value if the current node is a signed short, returns 0 otherwise.
 */
_Data_['getShort'] = function() {
    return _pn_data_get_short(this._data);
};

/**
 * @return value if the current node is an unsigned int, returns 0 otherwise.
 */
_Data_['getUnsignedInteger'] = function() {
    return _pn_data_get_uint(this._data);
};

/**
 * @return value if the current node is a signed int, returns 0 otherwise.
 */
_Data_['getInteger'] = function() {
    return _pn_data_put_int(this._data);
};

/**
 * @return value if the current node is a character, returns 0 otherwise.
 */
_Data_['getChar'] = function() {
console.log("getChar not properly implemented yet");
return "character";
    //return _pn_data_get_char(this._data);
};

/**
 * @return value if the current node is an unsigned long, returns 0 otherwise.
 */
_Data_['getUnsignedLong'] = function() {
    return _pn_data_get_ulong(this._data);
};

/**
 * @return value if the current node is a signed long, returns 0 otherwise.
 */
_Data_['getLong'] = function() {
console.log("getLong");
    // Getting the long is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to hold
    // the 64 bit number and Data.Long.toNumber() to convert it back into a
    // JavaScript number.
    var low = _pn_data_get_long(this._data);
    var high = tempRet0;
    var long = new Data.Long(low, high);
    long = long.toNumber();

console.log("Data.getLong() long = " + long);

    return long;
};

/**
 * @return value if the current node is a timestamp, returns 0 otherwise.
 */
_Data_['getTimestamp'] = function() {
console.log("getTimestamp not properly implemented yet");
return 123456;
    //return _pn_data_get_timestamp(this._data);
};

/**
 * @return value if the current node is a float, returns 0 otherwise.
 */
_Data_['getFloat'] = function() {
    return _pn_data_get_float(this._data);
};

/**
 * @return value if the current node is a double, returns 0 otherwise.
 */
_Data_['getDouble'] = function() {
    return _pn_data_get_double(this._data);
};

/**
 * @return value if the current node is a decimal32, returns 0 otherwise.
 */
_Data_['getDecimal32'] = function() {
console.log("getDecimal32 not properly implemented yet");
    return _pn_data_get_decimal32(this._data);
};

/**
 * @return value if the current node is a decimal64, returns 0 otherwise.
 */
_Data_['getDecimal64'] = function() {
console.log("getDecimal64 not properly implemented yet");
    return _pn_data_get_decimal64(this._data);
};

/**
 * @return value if the current node is a decimal128, returns 0 otherwise.
 */
_Data_['getDecimal128'] = function() {
console.log("getDecimal128 not properly implemented yet");
    return _pn_data_get_decimal128(this._data);
};

/**
 * @return value if the current node is a UUID, returns null otherwise.
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
    var uuid = new Data['UUID'](bytes);

    // Tidy up the memory that we allocated on emscripten's stack.
    Runtime.stackRestore(sp);

    return uuid;
};

/**
 * @return value if the current node is a Binary, returns null otherwise.
 */
_Data_['getBinary'] = function() {
console.log("getBinary not properly implemented yet");
    //this._check(_pn_data_put_binary(this._data, b));
};

/**
 * @return value if the current node is a String, returns "" otherwise.
 */
_Data_['getString'] = function() {
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


 * @return value if the current node is a Symbol, returns "" otherwise.
 */
_Data_['getSymbol'] = function() {
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


// TODO copy, format and dump




/**
 * Serialise a Native JavaScript Object into an AMQP Map.
 * @param object the Native JavaScript Object that we wish to serialise.
 */
_Data_['putDictionary'] = function(object) {
    this['putMap']();
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
 * @return the deserialised Native JavaScript Object.
 */
_Data_['getDictionary'] = function() {
    if (this['enter']()) {
        var result = {};
        while (this['next']()) {
            var key = this['getObject']();
            var value = null;
            if (this['next']()) {
                value = this['getObject']()
            }
            result[key] = value;
        }
        this['exit']();
        return result;
    }
};


/**
 * Serialise a Native JavaScript Array into an AMQP List.
 * @param array the Native JavaScript Array that we wish to serialise.
 */
_Data_['putJSArray'] = function(array) {
    this['putList']();
    this['enter']();
    for (var i = 0, len = array.length; i < len; i++) {
        this['putObject'](array[i])
    }
    this['exit']();
};

/**
 * Deserialise from an AMQP List into a Native JavaScript Array.
 * @return the deserialised Native JavaScript Array.
 */
_Data_['getJSArray'] = function() {
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
 * This method is the entry point for serialising native JavaScript types into
 * AMQP types. In an ideal world there would be a nice clean one to one mapping
 * and we could employ a look-up table but in practice the JavaScript type system
 * doesn't really lend itself to that and we have to employ extra checks,
 * heuristics and inferences.
 * @param obj the JavaScript Object or primitive to be serialised.
 */
_Data_['putObject'] = function(obj) {
console.log("Data.putObject");


console.log("obj = " + obj);
//console.log("typeof obj = " + (typeof obj));
//console.log("obj prototype = " + Object.prototype.toString.call(obj));

    if (obj == null) { // == Checks for null and undefined.
console.log("obj is null");
        this['putNull']();
    } else if (Data.isString(obj)) {
console.log("obj is String");

        var quoted = obj.match(/(['"])[^'"]*\1/);
        if (quoted) {
            quoted = quoted[0].slice(1, -1);
console.log("obj is quoted String " + quoted);
            this['putString'](quoted);
        } else {
            // TODO check for stringified numbers.
            this['putString'](obj);
        }   
    } else if (obj instanceof Data['UUID']) {
        this['putUUID'](obj);
    } else if (obj instanceof Data['Symbol']) {
        this['putSymbol'](obj);
    } else if (Data.isNumber(obj)) {
        /**
         * Encoding JavaScript numbers is surprisingly complex and has several
         * gotchas. The code here tries to do what the author believes is the
         * most intuitive encoding of the native JavaScript Number. It first
         * tries to identify if the number is an integer or floating point type
         * by checking if the number modulo 1 is zero (i.e. if it has a remainder
         * then it's a floating point type, which is encoded here as a double).
         * If the number is an integer type a test is made to check if it is a
         * 32 bit Int value. N.B. JavaScript automagically coerces floating
         * point numbers with a zero Fractional Part into an exact integer so
         * numbers like 1.0, 100.0 etc. will be encoded as int or long here,
         * which is unlikely to be what is wanted. There's no easy way around this
         * the two main options are to add a very small fractional number or to
         * represent the number in a String literal e.g. "1.0f", "1.0d", "1l"
         */
        if (obj % 1 === 0) {
console.log(obj + " is Integer Type " + (obj|0));
            if (obj === (obj|0)) { // the |0 coerces to a 32 bit value.
                // 32 bit integer - encode as an Integer.
console.log(obj + " is Int ");
                this['putInteger'](obj);
            } else { // Longer than 32 bit - encode as a Long.
console.log(obj + " is Long");
                this['putLong'](obj);
            }
        } else { // Floating point type - encode as a Double
console.log(obj + " is Float Type");
            this['putDouble'](obj);
        }
    } else if (Data.isBoolean(obj)) {
        this['putBoolean'](obj);
    } else if (Data.isArray(obj)) { // Native JavaScript Array
        this['putJSArray'](obj);
    } else {
        this['putDictionary'](obj);
    }
};
_Data_.putObject = _Data_['putObject'];


// TODO
_Data_['getObject'] = function(obj) {
console.log("Data.getObject: not fully implemented yet");

    var type = this.type();
console.log("type = " + type);

    var getter = Data.GetMappings[type];

console.log("getter = " + getter);

    return this[getter]();
};
_Data_.getObject = _Data_['getObject'];













/*
Module['Inflate'] = function(size) {
    var _public = {};
    var stream = _inflateInitialise();
    var inputBuffer  = Buffer(size);
    var outputBuffer = Buffer(size);

    // Public methods
    _public['destroy'] = function() {
        _inflateDestroy(stream);
        inputBuffer.destroy();
        outputBuffer.destroy();
    };

    _public['reset'] = function() {
        _inflateReset(stream);
    };

    _public['inflate'] = function(ptr) {
        ptr = ptr ? ptr : outputBuffer.getRaw();
        var inflatedSize; // Pass by reference variable - need to use Module.setValue to initialise it.
        setValue(inflatedSize, outputBuffer.size, 'i32');
        var err = _zinflate(stream, ptr, inflatedSize, inputBuffer.getRaw(), inputBuffer.size);
        inflatedSize = getValue(inflatedSize, 'i32'); // Dereference the real inflatedSize value;
        outputBuffer.setSize(inflatedSize);
        return ((err < 0) ? err : inflatedSize); // Return the inflated size, or error code if inflation fails.
    };

    // Export methods from the input and output buffers for use by client code.
    _public['setInputBufferSize'] = inputBuffer.setSize;
    _public['getRawInputBuffer'] = inputBuffer.getRaw;
    _public['getInputBuffer'] = inputBuffer.getBuffer;

    _public['setOutputBufferSize'] = outputBuffer.setSize;
    _public['getRawOutBuffer'] = outputBuffer.getRaw;
    _public['getOutputBuffer'] = outputBuffer.getBuffer;

    return _public;
};
*/


