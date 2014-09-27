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
/*                                 Messenger                                 */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.Messenger instance giving it an (optional) name. If name
 * is supplied that will be used as the name of the Messenger, otherwise a UUID
 * will be used. The Messenger is initialised to non-blocking mode as it makes
 * little sense to have blocking behaviour in a JavaScript implementation.
 * @classdesc This class is
 * @constructor proton.Messenger
 * @param {string} name the name of this Messenger instance.
 */
Module['Messenger'] = function(name) { // Messenger Constructor.
    /**
     * The emscripten idiom below is used in a number of places in the JavaScript
     * bindings to map JavaScript Strings to C style strings. ALLOC_STACK will
     * increase the stack and place the item there. When the stack is next restored
     * (by calling Runtime.stackRestore()), that memory will be automatically
     * freed. In C code compiled by emscripten saving and restoring of the stack
     * is automatic, but if we want to us ALLOC_STACK from native JavaScript we
     * need to explicitly save and restore the stack using Runtime.stackSave()
     * and Runtime.stackRestore() or we will leak emscripten heap memory.
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

    // Subscriptions that haven't yet completed, used for managing subscribe events.
    this._pendingSubscriptions = [];

    // Used in the Event registration mechanism (in the 'on' and 'emit' methods).
    this._callbacks = {};

    // This call ensures that the emscripten network callback functions are initialised.
    Module.EventDispatch.registerMessenger(this);


    // TODO improve error handling mechanism.
    /*
     * The emscripten websocket error event could get triggered by any Messenger
     * and it's hard to determine which one without knowing which file descriptors
     * are associated with which instance. As a workaround we set the _checkErrors
     * flag when we call put or subscribe and reset it when work succeeds.
     */
    this._checkErrors = false;

    /**
     * TODO update to handle multiple Messenger instances
     * Handle the emscripten websocket error and use it to trigger a MessengerError
     * Note that the emscripten websocket error passes an array containing the
     * file descriptor, the errno and the message, we just use the message here.
     */
    var that = this;
    Module['websocket']['on']('error', function(error) {

console.log("Module['websocket']['on'] caller is " + arguments.callee.caller.toString());

console.log("that._checkErrors = " + that._checkErrors);
console.log("error = " + error);
        if (that._checkErrors) {
            that._emit('error', new Module['MessengerError'](error[2]));
        }
    });
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

        if (this._callbacks['error']) {
            this._emit('error', new Module['MessengerError'](message));
        } else {
            throw new Module['MessengerError'](message);
        }
    } else {
        return code;
    }
};

/**
 * Invokes the callbacks registered for a specified event.
 * @method _emit
 * @memberof! proton.Messenger#
 * @param event {string} the event we want to emit.
 * @param param {object} the parameter we'd like to pass to the event callback.
 */
_Messenger_._emit = function(event, param) {
    var callbacks = this._callbacks[event];
    if (callbacks) {
        for (var i = 0; i < callbacks.length; i++) {
            var callback = callbacks[i];
            if ('function' === typeof callback) {
                callback.call(this, param);
            }
        }
    }
};

/**
 * Checks any pending subscriptions and when a source address becomes available
 * emit a subscription event passing the Subscription that triggered the event.
 * Note that this doesn't seem to work for listen/bind style subscriptions,
 * that is to say subscriptions of the form amqp://~0.0.0.0 don't know why?
 */
_Messenger_._checkSubscriptions = function() {
    // Check for completed subscriptions, and emit subscribe event.
    var subscriptions = this._pendingSubscriptions;
    if (subscriptions.length) {
        var pending = []; // Array of any subscriptions that remain pending.
        for (var j = 0; j < subscriptions.length; j++) {
            subscription = subscriptions[j];
            if (subscription['getAddress']()) {
                this._emit('subscription', subscription);
            } else {
                pending.push(subscription);
            }
        }
        this._pendingSubscriptions = pending;
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
 * Registers a listener callback for a specified event.
 * @method on
 * @memberof! proton.Messenger#
 * @param {string} event the event we want to listen for.
 * @param {function} callback the callback function to be registered for the specified event.
 */
_Messenger_['on'] = function(event, callback) {
    if ('function' === typeof callback) {
        if (!this._callbacks[event]) {
            this._callbacks[event] = [];
        }

        this._callbacks[event].push(callback);
    }
};

/**
 * Removes a listener callback for a specified event.
 * @method removeListener
 * @memberof! proton.Messenger#
 * @param {string} event the event we want to detach from.
 * @param {function} callback the callback function to be removed for the specified event.
 *        if no callback is specified all callbacks are removed for the event.
 */
_Messenger_['removeListener'] = function(event, callback) {
    if (callback) {
        var callbacks = this._callbacks[event];
        if ('function' === typeof callback && callbacks) {
            // Search for the specified callback.
            for (var i = 0; i < callbacks.length; i++) {
                if (callback === callbacks[i]) {
                    // If we find the specified callback delete it and return.
                    callbacks.splice(i, 1);
                    return;
                }
            }
        }
    } else {
        // If we call remove with no callback we remove all callbacks.
        delete this._callbacks[event];
    }
};

/**
 * Retrieves the name of a Messenger.
 * @method getName
 * @memberof! proton.Messenger#
 * @returns {string} the name of the messenger.
 */
_Messenger_['getName'] = function() {
    return Pointer_stringify(_pn_messenger_name(this._messenger));
};

/**
 * Retrieves the timeout for a Messenger.
 * @method getTimeout
 * @memberof! proton.Messenger#
 * @returns {number} zero because JavaScript is fundamentally non-blocking.
 */
_Messenger_['getTimeout'] = function() {
    return 0;
};

/**
 * Accessor for messenger blocking mode.
 * @method isBlocking
 * @memberof! proton.Messenger#
 * @returns {boolean} false because JavaScript is fundamentally non-blocking.
 */
_Messenger_['isBlocking'] = function() {
    return false;
};

/**
 * Free the Messenger. This will close all connections that are managed
 * by the Messenger. Call the stop method before destroying the Messenger.
 * <p>
 * N.B. This method has to be called explicitly in JavaScript as we can't
 * intercept finalisers, so we need to remember to free before removing refs.
 * @method free
 * @memberof! proton.Messenger#
 */
_Messenger_['free'] = function() {
    // This call ensures that the emscripten network callback functions are removed.
    Module.EventDispatch.unregisterMessenger(this);
    _pn_messenger_free(this._messenger);
};

/**
 * @method getErrno
 * @memberof! proton.Messenger#
 * @returns {number} the most recent error message code.
 */
_Messenger_['getErrno'] = function() {
    return _pn_messenger_errno(this._messenger);
};

/**
 * @method getError
 * @memberof! proton.Messenger#
 * @returns {string} the most recent error message as a String.
 */
_Messenger_['getError'] = function() {
    return Pointer_stringify(_pn_error_text(_pn_messenger_error(this._messenger)));
};

/**
 * Returns the size of the outgoing window that was set with setOutgoingWindow.
 * The default is 0.
 * @method getOutgoingWindow
 * @memberof! proton.Messenger#
 * @returns {number} the outgoing window size.
 */
_Messenger_['getOutgoingWindow'] = function() {
    return _pn_messenger_get_outgoing_window(this._messenger);
};

/**
 * Sets the outgoing tracking window for the Messenger. The Messenger will
 * track the remote status of this many outgoing deliveries after calling
 * send. Defaults to zero.
 * <p>
 * A Message enters this window when you call put() with the Message.
 * If your outgoing window size is n, and you call put() n+1 times, status
 * information will no longer be available for the first Message.
 * @method setOutgoingWindow
 * @memberof! proton.Messenger#
 * @param {number} window the size of the tracking window in messages.
 */
_Messenger_['setOutgoingWindow'] = function(window) {
    this._check(_pn_messenger_set_outgoing_window(this._messenger, window));
};

/**
 * Returns the size of the incoming window that was set with setIncomingWindow.
 * The default is 0.
 * @method getIncomingWindow
 * @memberof! proton.Messenger#
 * @returns {number} the incoming window size.
 */
_Messenger_['getIncomingWindow'] = function() {
    return _pn_messenger_get_incoming_window(this._messenger);
};

/**
 * Sets the incoming tracking window for the Messenger. The Messenger will
 * track the remote status of this many incoming deliveries after calling
 * send. Defaults to zero.
 * <p>
 * Messages enter this window only when you take them into your application
 * using get(). If your incoming window size is n, and you get() n+1 messages
 * without explicitly accepting or rejecting the oldest message, then the
 * Message that passes beyond the edge of the incoming window will be assigned
 * the default disposition of its link.
 * @method setIncomingWindow
 * @memberof! proton.Messenger#
 * @param {number} window the size of the tracking window in messages.
 */
_Messenger_['setIncomingWindow'] = function(window) {
    this._check(_pn_messenger_set_incoming_window(this._messenger, window));
};

/**
 * Currently a no-op placeholder. For future compatibility, do not send or
 * recv messages before starting the Messenger.
 * @method start
 * @memberof! proton.Messenger#
 */
_Messenger_['start'] = function() {
    this._check(_pn_messenger_start(this._messenger));
};

/**
 * Transitions the Messenger to an inactive state. An inactive Messenger
 * will not send or receive messages from its internal queues. A Messenger
 * should be stopped before being discarded to ensure a clean shutdown
 * handshake occurs on any internally managed connections.
 * <p>
 * The Messenger may require some time to stop if it is busy, and in that
 * case will return {@link proton.Error.INPROGRESS}. In that case, call isStopped
 * to see if it has fully stopped.
 * @method stop
 * @memberof! proton.Messenger#
 * @returns {@link proton.Error.INPROGRESS} if still busy.
 */
_Messenger_['stop'] = function() {
    return this._check(_pn_messenger_stop(this._messenger));
};

/**
 * Returns true iff a Messenger is in the stopped state.
 * @method isStopped
 * @memberof! proton.Messenger#
 * @returns {boolean} true iff a Messenger is in the stopped state.
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
 * @method subscribe
 * @memberof! proton.Messenger#
 * @param {string} source the source address we're subscribing to.
 * @returns {Subscription} a subscription.
 */
_Messenger_['subscribe'] = function(source) {
    if (!source) {
        this._check(Module['Error']['ARG_ERR']);
    }
    var sp = Runtime.stackSave();
    this._checkErrors = true; // TODO improve error handling mechanism.
    var subscription = _pn_messenger_subscribe(this._messenger,
                                               allocate(intArrayFromString(source), 'i8', ALLOC_STACK));
    Runtime.stackRestore(sp);

    if (!subscription) {
        this._check(Module['Error']['ERR']);
    }

    // For peer subscriptions to this Messenger emit a subscription event
    // immediately otherwise defer until the address is resolved remotely.
    if (source.indexOf('~') !== -1) {
        subscription = new Subscription(subscription, source);
        this._emit('subscription', subscription);
    } else {
        subscription = new Subscription(subscription)
        this._pendingSubscriptions.push(subscription);
    }
    return subscription;
};

/**
 * Places the content contained in the message onto the outgoing queue
 * of the Messenger. This method will never block, however it will send any
 * unblocked Messages in the outgoing queue immediately and leave any blocked
 * Messages remaining in the outgoing queue. The outgoing property may be
 * used to check the depth of the outgoing queue.
 * <p>
 * When the content in a given Message object is copied to the outgoing
 * message queue, you may then modify or discard the Message object
 * without having any impact on the content in the outgoing queue.
 * <p>
 * This method returns an outgoing tracker for the Message.  The tracker
 * can be used to determine the delivery status of the Message.
 * @method put
 * @memberof! proton.Messenger#
 * @param {proton.Message} message a Message to send.
 * @param {boolean} flush if this is set true or is undefined then messages are
 *        flushed (this is the default). If explicitly set to false then messages
 *        may not be sent immediately and might require an explicit call to work().
 *        This may be used to "batch up" messages and *may* be more efficient.
 * @returns {proton.Data.Long} a tracker.
 */
_Messenger_['put'] = function(message, flush) {
    flush = flush === false ? false : true;
    message._preEncode();
    this._checkErrors = true; // TODO improve error handling mechanism.
    this._check(_pn_messenger_put(this._messenger, message._message));

    // If flush is set invoke pn_messenger_work.
    if (flush) {
        _pn_messenger_work(this._messenger, 0);
    }

    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var low = _pn_messenger_outgoing_tracker(this._messenger);
    var high = Runtime.getTempRet0();
    return new Data.Long(low, high);
};

/**
 * Gets the last known remote state of the delivery associated with the given tracker.
 * @method status
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker whose status is to be retrieved.
 * @returns {proton.Status} one of None, PENDING, REJECTED, or ACCEPTED.
 */
_Messenger_['status'] = function(tracker) {
    if (tracker == null) {
        var low = _pn_messenger_outgoing_tracker(this._messenger);
        var high = Runtime.getTempRet0();
        tracker = new Data.Long(low, high);
    }

    return _pn_messenger_status(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits());
};

/**
 * Checks if the delivery associated with the given tracker is still waiting to be sent.
 * @method isBuffered
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker identifying the delivery.
 * @returns {boolean} true if delivery is still buffered.
 */
_Messenger_['isBuffered'] = function(tracker) {
    if (tracker == null) {
        var low = _pn_messenger_outgoing_tracker(this._messenger);
        var high = Runtime.getTempRet0();
        tracker = new Data.Long(low, high);
    }

    return (_pn_messenger_buffered(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits()) > 0);
};

/**
 * Frees a Messenger from tracking the status associated with a given tracker.
 * If you don't supply a tracker, all outgoing messages up to the most recent
 * will be settled.
 * @method settle
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker identifying the delivery.
 */
_Messenger_['settle'] = function(tracker) {
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_outgoing_tracker(this._messenger);
        var high = Runtime.getTempRet0();
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_settle(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Sends or receives any outstanding messages queued for a Messenger.
 * For JavaScript the only timeout that makes sense is 0 (do not block).
 * This method may also do I/O work other than sending and receiving messages.
 * For example, closing connections after messenger.stop() has been called.
 * @method work
 * @memberof! proton.Messenger#
 * @returns {boolean} true if there is work still to do, false otherwise.
 */
_Messenger_['work'] = function() {
    var err = _pn_messenger_work(this._messenger, 0);
    if (err === Module['Error']['TIMEOUT']) {
console.log("work = false");
        return false;
    } else {
        this._checkErrors = false; // TODO improve error handling mechanism.
        this._check(err);
console.log("work = true");
        return true;
    }
};

/**
 * Receives up to limit messages into the incoming queue.  If no value for limit
 * is supplied, this call will receive as many messages as it can buffer internally.
 * @method recv
 * @memberof! proton.Messenger#
 * @param {number} limit the maximum number of messages to receive or -1 to to receive
 *        as many messages as it can buffer internally.
 */
_Messenger_['recv'] = function(limit) {
    this._check(_pn_messenger_recv(this._messenger, (limit ? limit : -1)));
};

/**
 * Returns the capacity of the incoming message queue of messenger. Note this
 * count does not include those messages already available on the incoming queue.
 * @method receiving
 * @memberof! proton.Messenger#
 * @returns {number} the message queue capacity.
 */
_Messenger_['receiving'] = function() {
    return _pn_messenger_receiving(this._messenger);
};

/**
 * Moves the message from the head of the incoming message queue into the
 * supplied message object. Any content in the message will be overwritten.
 * <p>
 * A tracker for the incoming Message is returned. The tracker can later be
 * used to communicate your acceptance or rejection of the Message.
 * @method get
 * @memberof! proton.Messenger#
 * @param {proton.Message} message the destination message object. If no Message
 *        object is supplied, the Message popped from the head of the queue is discarded.
 * @param {boolean} decodeBinaryAsString if set decode any AMQP Binary payload
 *        objects as strings. This can be useful as the data in Binary objects
 *        will be overwritten with subsequent calls to get, so they must be
 *        explicitly copied. Needless to say it is only safe to set this flag if
 *        you know that the data you are dealing with is actually a string, for
 *        example C/C++ applications often seem to encode strings as AMQP binary,
 *        a common cause of interoperability problems.
 * @returns {proton.Data.Long} a tracker for the incoming Message.
 */
_Messenger_['get'] = function(message, decodeBinaryAsString) {
    var impl = null;
    if (message) {
        impl = message._message;
    }

    this._check(_pn_messenger_get(this._messenger, impl));

    if (message) {
        message._postDecode(decodeBinaryAsString);
    }

    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var low = _pn_messenger_incoming_tracker(this._messenger);
    var high = Runtime.getTempRet0();

    return new Data.Long(low, high);
};

/**
 * Returns the Subscription of the Message returned by the most recent call
 * to get, or null if pn_messenger_get has not yet been called.
 * @method incomingSubscription
 * @memberof! proton.Messenger#
 * @returns {Subscription} a Subscription or null if get has never been called
 *          for this Messenger.
 */
_Messenger_['incomingSubscription'] = function() {
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
 * @method accept
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker identifying the delivery.
 */
_Messenger_['accept'] = function(tracker) {
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_incoming_tracker(this._messenger);
        var high = Runtime.getTempRet0();
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_accept(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Rejects the Message indicated by the tracker.  If no tracker is supplied,
 * all messages that have been returned by the get method are rejected, except
 * those already auto-settled by passing beyond your outgoing window size.
 * @method reject
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker identifying the delivery.
 */
_Messenger_['reject'] = function(tracker) {
    // Getting the tracker is a little tricky as it is a 64 bit number. The way
    // emscripten handles this is to return the low 32 bits directly and pass
    // the high 32 bits via the tempRet0 variable. We use Data.Long to pass the
    // low/high pair around to methods that require a tracker.
    var flags = 0;
    if (tracker == null) {
        var low = _pn_messenger_incoming_tracker(this._messenger);
        var high = Runtime.getTempRet0();
        tracker = new Data.Long(low, high);
        flags = Module['Messenger'].PN_CUMULATIVE;
    }

    this._check(_pn_messenger_reject(this._messenger, tracker.getLowBitsUnsigned(), tracker.getHighBits(), flags));
};

/**
 * Returns the number of messages in the outgoing message queue of a messenger.
 * @method outgoing
 * @memberof! proton.Messenger#
 * @returns {number} the outgoing queue depth.
 */
_Messenger_['outgoing'] = function() {
    return _pn_messenger_outgoing(this._messenger);
};

/**
 * Returns the number of messages in the incoming message queue of a messenger.
 * @method incoming
 * @memberof! proton.Messenger#
 * @returns {number} the incoming queue depth.
 */
_Messenger_['incoming'] = function() {
    return _pn_messenger_incoming(this._messenger);
};

/**
 * Adds a routing rule to a Messenger's internal routing table.
 * <p>
 * The route method may be used to influence how a messenger will internally treat
 * a given address or class of addresses. Every call to the route method will
 * result in messenger appending a routing rule to its internal routing table.
 * <p>
 * Whenever a message is presented to a messenger for delivery, it will match the
 * address of this message against the set of routing rules in order. The first
 * rule to match will be triggered, and instead of routing based on the address
 * presented in the message, the messenger will route based on the address supplied
 * in the rule.
 * <p>
 * The pattern matching syntax supports two types of matches, a '' will match any
 * character except a '/', and a '*' will match any character including a '/'.
 * <p>
 * A routing address is specified as a normal AMQP address, however it may
 * additionally use substitution variables from the pattern match that triggered
 * the rule.
 * <p>
 * Any message sent to "foo" will be routed to "amqp://foo.com":
 * <pre>
 * route("foo", "amqp://foo.com");
 * </pre>
 * Any message sent to "foobar" will be routed to "amqp://foo.com/bar":
 * <pre>
 * route("foobar", "amqp://foo.com/bar");
 * </pre>
 * Any message sent to bar/<path> will be routed to the corresponding path within
 * the amqp://bar.com domain:
 * <pre>
 * route("bar/*", "amqp://bar.com/$1");
 * </pre>
 * Supply credentials for foo.com:
 * <pre>
 * route("amqp://foo.com/*", "amqp://user:password@foo.com/$1");
 * </pre>
 * Supply credentials for all domains:
 * <pre>
 * route("amqp://*", "amqp://user:password@$1");
 * </pre>
 * Route all addresses through a single proxy while preserving the original destination:
 * <pre>
 * route("amqp://%/*", "amqp://user:password@proxy/$1/$2");
 * </pre>
 * Route any address through a single broker:
 * <pre>
 * route("*", "amqp://user:password@broker/$1");
 * </pre>
 * @method route
 * @memberof! proton.Messenger#
 * @param {string} pattern a glob pattern to select messages.
 * @param {string} address an address indicating outgoing address rewrite.
 */
_Messenger_['route'] = function(pattern, address) {
    var sp = Runtime.stackSave();
    this._check(_pn_messenger_route(this._messenger,
                                    allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                    allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

/**
 * Rewrite message addresses prior to transmission.
 * <p>
 * Similar to route(), except that the destination of the Message is determined
 * before the message address is rewritten.
 * <p>
 * The outgoing address is only rewritten after routing has been finalized. If
 * a message has an outgoing address of "amqp://0.0.0.0:5678", and a rewriting
 * rule that changes its outgoing address to "foo", it will still arrive at the
 * peer that is listening on "amqp://0.0.0.0:5678", but when it arrives there,
 * the receiver will see its outgoing address as "foo".
 * <p>
 * The default rewrite rule removes username and password from addresses
 * before they are transmitted.
 * @method rewrite
 * @memberof! proton.Messenger#
 * @param {string} pattern a glob pattern to select messages.
 * @param {string} address an address indicating outgoing address rewrite.
 */
_Messenger_['rewrite'] = function(pattern, address) {
    var sp = Runtime.stackSave();
    this._check(_pn_messenger_rewrite(this._messenger,
                                      allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                      allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    Runtime.stackRestore(sp);
};

