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
 * It will be used to wrap the emscripten compiled proton-c code and be minified by
 * the Closure compiler, so all comments will be stripped from the actual library.
 * <p>
 * This JavaScript wrapper provides a somewhat more idiomatic object oriented
 * interface which abstracts the low-level emscripten based implementation details
 * from client code. Any similarities to the Proton Python binding are deliberate.
 * @file
 */

/**
 * The Module Object is exported by emscripten for all execution platforms, we
 * use it as a namespace to allow us to selectively export only what we wish to
 * be publicly visible from this package/module. We will use the associative
 * array form for declaring exported properties to prevent the Closure compiler
 * from minifying e.g. <pre>Module['Messenger'] = ...</pre>
 * Exported Objects can be used in client code using the appropriate namespace:
 * <pre>
 * proton = require('proton.js');
 * var messenger = new proton.Messenger();
 * var message = new proton.Message();
 * </pre>
 * @namespace proton
 */

var Module = {
    // Prevent emscripten runtime exiting, we will be enabling network callbacks.
    'noExitRuntime' : true,
};

/*****************************************************************************/
/*                                                                           */
/*                                   Status                                  */
/*                                                                           */
/*****************************************************************************/

/**
 * Export Status Enum, avoiding minification.
 * @enum
 * @alias Status
 * @memberof proton
 */
Module['Status'] = {
    /** PN_STATUS_UNKNOWN */  'UNKNOWN':  0, // The tracker is unknown.
    /** PN_STATUS_PENDING */  'PENDING':  1, // The message is in flight.
                                             // For outgoing messages, use messenger.isBuffered()
                                             // to see if it has been sent or not.
    /** PN_STATUS_ACCEPTED */ 'ACCEPTED': 2, // The message was accepted.
    /** PN_STATUS_REJECTED */ 'REJECTED': 3, // The message was rejected.
    /** PN_STATUS_RELEASED */ 'RELEASED': 4, // The message was released.
    /** PN_STATUS_MODIFIED */ 'MODIFIED': 5, // The message was modified.
    /** PN_STATUS_ABORTED */  'ABORTED':  6, // The message was aborted.
    /** PN_STATUS_SETTLED */  'SETTLED':  7  // The remote party has settled the message.
};


/*****************************************************************************/
/*                                                                           */
/*                                   Error                                   */
/*                                                                           */
/*****************************************************************************/

/**
 * Export Error Enum, avoiding minification.
 * @enum
 * @alias Error
 * @memberof proton
 */
Module['Error'] = {
    /** PN_EOS */        'EOS':        -1,
    /** PN_ERR */        'ERR':        -2,
    /** PN_OVERFLOW */   'OVERFLOW':   -3,
    /** PN_UNDERFLOW */  'UNDERFLOW':  -4,
    /** PN_STATE_ERR */  'STATE_ERR':  -5,
    /** PN_ARG_ERR */    'ARG_ERR':    -6,
    /** PN_TIMEOUT */    'TIMEOUT':    -7,
    /** PN_INTR */       'INTR':       -8,
    /** PN_INPROGRESS */ 'INPROGRESS': -9
};


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
 * @returns {proton.Data.Long} a tracker.
 */
_Messenger_['put'] = function(message) {
    message._preEncode();
    this._check(_pn_messenger_put(this._messenger, message._message));

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
console.log("settle: not fully tested yet");
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
 * For JavaScript the only timeout that makes sense is 0 == do not block.
 * This method may also do I/O work other than sending and receiving messages.
 * For example, closing connections after messenger.stop() has been called.
 * @method work
 * @memberof! proton.Messenger#
 * @returns {number} 0 if no work to do, < 0 if error, or 1 if work was done.
 */
_Messenger_['work'] = function() {
    return _pn_messenger_work(this._messenger, 0);
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
 * @returns {proton.Data.Long} a tracker for the incoming Message.
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
    var high = Runtime.getTempRet0();
console.log("get low = " + low);
console.log("get high = " + high);

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
 * @method accept
 * @memberof! proton.Messenger#
 * @param {proton.Data.Long} tracker the tracker identifying the delivery.
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
console.log("reject: not fully tested yet");
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
 * 
 * @method route
 * @memberof! proton.Messenger#
 * @param {string} pattern a glob pattern to select messages.
 * @param {string} address an address indicating outgoing address rewrite.
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
 * Constructs a Subscription instance.
 * @classdesc This class is a wrapper for Messenger's subscriptions.
 * Subscriptions should never be *directly* instantiated by client code only via
 * Messenger.subscribe() or Messenger.incomingSubscription(), so we declare the
 * constructor in the scope of the package and don't export it via Module.
 * @constructor Subscription                                                              
 */
var Subscription = function(subscription) { // Subscription Constructor.
    this._subscription = subscription;
};

/**
 * TODO Not sure exactly what pn_subscription_get_context does.
 * @method getContext
 * @memberof! Subscription#
 * @returns the Subscription's Context.
 */
Subscription.prototype['getContext'] = function() {
    return _pn_subscription_get_context(this._subscription);
};

/**
 * TODO Not sure exactly what pn_subscription_set_context does.
 * @method setContext
 * @memberof! Subscription#
 * @param context the Subscription's new Context.
 */
Subscription.prototype['setContext'] = function(context) {
    _pn_subscription_set_context(this._subscription, context);
};

/**
 * @method getAddress
 * @memberof! Subscription#
 * @returns the Subscription's Address.
 */
Subscription.prototype['getAddress'] = function() {
    return Pointer_stringify(_pn_subscription_address(this._subscription));
};


/*****************************************************************************/
/*                                                                           */
/*                                  Message                                  */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.Message instance.
 * @classdesc This class is
 * @constructor proton.Message
 */
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
    this['properties'] = {};

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
    // A Message Object may be reused so we create new Data instances and clear
    // the state for them each time put() gets called.
    var inst = new Data(_pn_message_instructions(this._message));
    var ann = new Data(_pn_message_annotations(this._message));
    var props = new Data(_pn_message_properties(this._message));
    var body = new Data(_pn_message_body(this._message));

    inst.clear();
    if (this['instructions']) {
console.log("Encoding instructions");
        inst['putObject'](this['instructions']);
    }

    ann.clear();
    if (this['annotations']) {
console.log("Encoding annotations");
        ann['putObject'](this['annotations']);
    }

    props.clear();
    if (this['properties']) {
console.log("Encoding properties");
        props['putObject'](this['properties']);
    }

    body.clear();
    if (this['body']) {
console.log("Encoding body");
        body['putObject'](this['body']);
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
        this['body'] = body['getObject']();
    } else {
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
 */
Module['Data'] = function(data) { // Data Constructor.
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
 * @property {Array<String>} TypeNames
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

// **************************** Inner Classes *********************************

// --------------------------- proton.Data.Uuid -------------------------------
/**
 * Create a proton.Data.Uuid which is a type 4 UUID.
 * @classdesc
 * This class represents a type 4 UUID, wich may use crypto libraries to generate
 * the UUID if supported by the platform (e.g. node.js or a modern browser)
 * @constructor proton.Data.Uuid
 * @param u a UUID. If null a type 4 UUID is generated wich may use crypto if
 *        supported by the platform. If u is an emscripten "pointer" we copy the
 *        data from that. If u is a JavaScript Array we use it as-is. If u is a
 *        String then we try to parse that as a UUID.
 * @property {Array} uuid is the compact array form of the UUID.
 */
Data['Uuid'] = function(u) { // Data.Uuid Constructor.
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
 * Returns the string representation of the proton.Data.Uuid.
 * @method toString
 * @memberof! proton.Data.Uuid#
 * @returns {string} the String
 *          /[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}/
 *          form of a {@link proton.Data.Uuid}.
 */
Data['Uuid'].prototype.toString = Data['Uuid'].prototype.valueOf = function() {
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
 * Compare two instances of proton.Data.Uuid for equality.
 * @method equals
 * @memberof! proton.Data.Uuid#
 * @param {proton.Data.Uuid} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Uuid'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};

// ---------------------------- proton.Data.Symbol ---------------------------- 
/**
 * Create a proton.Data.Symbol.
 * @classdesc
 * This class represents an AMQP Symbol. This class is necessary primarily as a
 * way to enable us to distinguish between a native String and a Symbol in the
 * JavaScript type system.
 * @constructor proton.Data.Symbol
 * @param {string} s a symbol.
 */
Data['Symbol'] = function(s) { // Data.Symbol Constructor.
    this.value = s;
};

/**
 * @method toString
 * @memberof! proton.Data.Symbol#
 * @returns {string} the String form of a {@link proton.Data.Symbol}.
 */
Data['Symbol'].prototype.toString = Data['Symbol'].prototype.valueOf = function() {
    return this.value;
};

/**
 * Compare two instances of proton.Data.Symbol for equality.
 * @method equals
 * @memberof! proton.Data.Symbol#
 * @param {proton.Data.Symbol} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Symbol'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};

// ---------------------------- proton.Data.Described ---------------------------- 
/**
 * Create a proton.Data.Described.
 * @classdesc
 * This class represents an AMQP Described.
 * @constructor proton.Data.Described
 * @param {object} value the value of the described type.
 * @param {string} descriptor an optional string describing the type.
 * @property {object} value the actual value of the described type.
 * @property {string} descriptor a string describing the type.
 */
Data['Described'] = function(value, descriptor) { // Data.Described Constructor.
    this['value'] = value;
    this['descriptor'] = descriptor;
};

/**
 * @method toString
 * @memberof! proton.Data.Described#
 * @returns {string} the String form of a {@link proton.Data.Described}.
 */
Data['Described'].prototype.toString = function() {
    return 'Described(' + this['value'] + ', ' + this['descriptor'] + ')';
};

/**
 * @method valueOf
 * @memberof! proton.Data.Described#
 * @returns {object} the value of the {@link proton.Data.Described}.
 */
Data['Described'].prototype.valueOf = function() {
    return this['value'];
};

/**
 * Compare two instances of proton.Data.Described for equality.
 * @method equals
 * @memberof! proton.Data.Described#
 * @param {proton.Data.Described} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Described'].prototype['equals'] = function(rhs) {
    if (rhs instanceof Data['Described']) {
        return ((this['descriptor'] === rhs['descriptor']) && (this['value'] === rhs['value']));
    } else {
        return false;
    }
};

// ---------------------------- proton.Data.Array ---------------------------- 
/**
 * TODO make this behave more like a native JavaScript Array: http://www.bennadel.com/blog/2292-extending-javascript-arrays-while-keeping-native-bracket-notation-functionality.htm
 * Create a proton.Data.Array.
 * @classdesc
 * This class represents an AMQP Array.
 * @constructor proton.Data.Array
 * @param {{string|number)} type the type of the Number either as a string or number.
 *        Stored internally as a string corresponding to one of the TypeNames.       
 * @param {Array|TypedArray} elements the Native JavaScript Array or TypedArray that we wish to serialise.
 * @param {object} descriptor an optional object describing the type.
 */
Data['Array'] = function(type, elements, descriptor) { // Data.Array Constructor.
    // This block caters for an empty Array or a Described empty Array.
    if (arguments.length < 2) {
        descriptor = type;
        type = 'NULL';
        elements = [];
    }

    this['type']  = (typeof type === 'number') ? Data['TypeNames'][type] : type;
    this['elements'] = elements;
    this['descriptor'] = descriptor;
};

/**
 * @method toString
 * @memberof! proton.Data.Array#
 * @returns {string} the String form of a {@link proton.Data.Array}.
 */
Data['Array'].prototype.toString = function() {
    var descriptor = (this['descriptor'] == null) ? '' : ':' + this['descriptor'];
    return this['type'] + 'Array' + descriptor + '[' + this['elements'] + ']';
};

/**
 * @method valueOf
 * @memberof! proton.Data.Array#
 * @returns {Array} the elements of the {@link proton.Data.Array}.
 */
Data['Array'].prototype.valueOf = function() {
    return this['elements'];
};

/**
 * Compare two instances of proton.Data.Array for equality. N.B. this method
 * compares the value of every Array element so its performance is O(n).
 * @method equals
 * @memberof! proton.Data.Array#
 * @param {proton.Data.Array} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Array'].prototype['equals'] = function(rhs) {
    if (rhs instanceof Data['Array'] &&
        // Check the string value of the descriptors.
        (('' + this['descriptor']) === ('' + rhs['descriptor'])) &&
        (this['type'] === rhs['type'])) {
        var elements = this['elements'];
        var relements = rhs['elements'];
        var length = elements.length;
        if (length === relements.length) {
            for (var i = 0; i < length; i++) {
                if (elements[i].valueOf() !== relements[i].valueOf()) {
                    return false;
                }
            }
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
};

// ---------------------- JavaScript Number Extensions ------------------------ 

Number.prototype['ubyte'] = function() {
    return new Data.TypedNumber('UBYTE', this);
};

Number.prototype['byte'] = function() {
    return new Data.TypedNumber('BYTE', this);
};

Number.prototype['ushort'] = function() {
    return new Data.TypedNumber('USHORT', this);
};

Number.prototype['short'] = function() {
    return new Data.TypedNumber('SHORT', this);
};

Number.prototype['uint'] = function() {
    return new Data.TypedNumber('UINT', this);
};

Number.prototype['int'] = function() {
    return new Data.TypedNumber('INT', this);
};

Number.prototype['ulong'] = function() {
    return new Data.TypedNumber('ULONG', this);
};

Number.prototype['long'] = function() {
    return new Data.TypedNumber('LONG', this);
};

Number.prototype['float'] = function() {
    return new Data.TypedNumber('FLOAT', this);
};

Number.prototype['double'] = function() {
    return new Data.TypedNumber('DOUBLE', this);
};

Number.prototype['char'] = function() {
    return new Data.TypedNumber('CHAR', this);
};

String.prototype['char'] = function() {
    return new Data.TypedNumber('CHAR', this.charCodeAt(0));
};

// ------------------------- proton.Data.TypedNumber -------------------------- 
/**
 * Create a proton.Data.TypedNumber.
 * @classdesc
 * This class is a simple wrapper class that allows a "type" to be recorded for
 * a number. The idea is that the JavaScript Number class is extended with extra
 * methods to allow numbers to be "modified" to TypedNumbers, so for example
 * 1.0.float() would modify 1.0 by returning a TypedNumber with type = FLOAT
 * and value = 1. The strings used for type correspond to the names of the Data
 * put* methods e.g. UBYTE, BYTE, USHORT, SHORT, UINT, INT, ULONG, LONG, FLOAT,
 * DOUBLE, CHAR so that the correct method to call can be derived from the type.
 * @constructor proton.Data.TypedNumber
 * @param {(string|number)} type the type of the Number either as a string or number.
 *        Stored internally as a string corresponding to one of the TypeNames.
 * @param {number} value the value of the Number.
 */
// Use dot notation as it is a "protected" inner class not exported from the closure.
Data.TypedNumber = function(type, value) { // Data.TypedNumber Constructor.
    this.type  = (typeof type === 'number') ? Data['TypeNames'][type] : type;
    this.value = value;
};

/**
 * @method toString
 * @memberof! proton.Data.TypedNumber#
 * @returns {string} the String form of a {@link proton.Data.TypedNumber}.
 */
Data.TypedNumber.prototype.toString = Data.TypedNumber.prototype.valueOf = function() {
    return +this.value;
};

// ----------------------------- proton.Data.Long ----------------------------- 
/**
 * Create a proton.Data.Long.
 * @classdesc
 * This class represents a 64 bit Integer value. It is used primarily to pass and
 * return 64 bit Integer values to and from the emscripten compiled proton-c library.
 * This class is needed because JavaScript cannot natively represent 64 bit
 * Integers with sufficient accuracy.
 * @constructor proton.Data.Long
 * @param {number} low the least significant word.
 * @param {number} high the most significant word.
 */
// Use dot notation as it is a "protected" inner class not exported from the closure.
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

/**
 * @method fromNumber
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} an instance of {@link proton.Data.Long} created
 *          using a native JavaScript number.
 */
Data.Long.fromNumber = function(value) {
    if (isNaN(value) || !isFinite(value)) {
        return Data.Long.ZERO;
    } else if (value <= -Data.Long.TWO_PWR_63_DBL_) {
        return Data.Long.MIN_VALUE;
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

/**
 * Return the twos complement of this instance.
 * @method negate
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} the twos complement of this instance.
 */
Data.Long.prototype.negate = function() {
    if (this.equals(Data.Long.MIN_VALUE)) {
        return Data.Long.MIN_VALUE;
    } else {
        return this.not().add(Data.Long.ONE);
    }
};

/**
 * Add two instances of {@link proton.Data.Long}.
 * @method add
 * @memberof! proton.Data.Long#
 * @param {proton.Data.Long} rhs the instance we wish to add to this instance.
 * @returns {proton.Data.Long} the sum of this value and the rhs.
 */
Data.Long.prototype.add = function(rhs) {
    // Divide each number into 4 chunks of 16 bits, and then sum the chunks.

    var a48 = this.high >>> 16;
    var a32 = this.high & 0xFFFF;
    var a16 = this.low >>> 16;
    var a00 = this.low & 0xFFFF;

    var b48 = rhs.high >>> 16;
    var b32 = rhs.high & 0xFFFF;
    var b16 = rhs.low >>> 16;
    var b00 = rhs.low & 0xFFFF;

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

/**
 * Return the complement of this instance.
 * @method not
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} the complement of this instance.
 */
Data.Long.prototype.not = function() {
    return new Data.Long(~this.low, ~this.high);
};

/**
 * Compare two instances of {@link proton.Data.Long} for equality.
 * @method equals
 * @memberof! proton.Data.Long#
 * @param {proton.Data.Long} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data.Long.prototype.equals = function(other) {
    return (this.high == other.high) && (this.low == other.low);
};

/**
 * @method getHighBits
 * @memberof! proton.Data.Long#
 * @returns {number} the most significant word of a {@link proton.Data.Long}.
 */
Data.Long.prototype.getHighBits = function() {
    return this.high;
};

/**
 * @method getLowBits
 * @memberof! proton.Data.Long#
 * @returns {number} the least significant word of a {@link proton.Data.Long}.
 */
Data.Long.prototype.getLowBits = function() {
    return this.low;
};

/**
 * @method getLowBitsUnsigned
 * @memberof! proton.Data.Long#
 * @returns {number} the least significant word of a {@link proton.Data.Long}
 *          as an unsigned value.
 */
Data.Long.prototype.getLowBitsUnsigned = function() {
    return (this.low >= 0) ? this.low : Data.Long.TWO_PWR_32_DBL_ + this.low;
};

/**
 * @method toNumber
 * @memberof! proton.Data.Long#
 * @returns {number} a native JavaScript number (with possible loss of precision).
 */
Data.Long.prototype.toNumber = function() {
    return (this.high * Data.Long.TWO_PWR_32_DBL_) + this.getLowBitsUnsigned();
};

/**
 * @method toString
 * @memberof! proton.Data.Long#
 * @returns {string} the String form of a {@link proton.Data.Long}.
 */
Data.Long.prototype.toString = function() {
    return this.high + ':' + this.getLowBitsUnsigned();
};

// ---------------------------- proton.Data.Binary ----------------------------
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
 * created by {@link proton.Data.getBINARY}. In this case the Binary is simply a
 * "view" onto the bytes owned by the Data instance. A client application can
 * safely access the bytes from the view, but if it wishes to use the bytes beyond
 * the scope of the Data instance (e.g. after the next {@link proton.Messenger.get}
 * call then the client must explicitly *copy* the bytes into a new buffer, for
 * example via copyBuffer().
 * <p>
 * Most of the {@link proton.Data} methods that take {@link proton.Data.Binary}
 * as a parameter "consume" the underlying data and take responsibility for
 * freeing it from the heap {@link proton.Data.putBINARY} {@link proton.Data.decode}.
 * For the methods that return a {@link proton.Data.Binary} the call to
 * {@link proton.Data.getBINARY}, which is the most common case, returns a Binary
 * that has a "view" of the underlying data that is actually owned by the Data
 * instance and thus doesn't need to be explicitly freed on the Binary. The call
 * to {@link proton.Data.encode} however returns a Binary that represents a *copy*
 * of the underlying data, in this case (like a client calling new proton.Data.Binary)
 * the client takes responsibility for freeing the data, unless of course it is
 * subsequently passed to a method that will consume the data (putBINARY/decode).
 * @constructor proton.Data.Binary
 * @param {(number|string|Array|TypedArray)} value. If value is a number then it 
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
 * a client wants to retain the data then copyBuffer should be used to explicitly
 * create a copy of the data which the client then owns to do with as it wishes.
 * @method getBuffer
 * @memberof! proton.Data.Binary#
 */
Data['Binary'].prototype['getBuffer'] = function() {
    return new Uint8Array(HEAPU8.buffer, this.start, this.size);
};

/**
 * Explicitly create a *copy* of the Binary copying the underlying binary data too.
 * @method copy
 * @param {number} offset an optional offset into the underlying buffer from
 *        where we want to copy the data, default is zero.
 * @param {number} n an optional number of bytes to copy, default is this.size - offset.
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
 * @method type
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
 * @returns {proton.Data.Binary} a Binary contianing the remaining bytes or null
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
 * @param {Date} d a Date value.
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
    return (long > 0) ? long : Data.Long.TWO_PWR_64_DBL_ + long;
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

    return binary;
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
console.log("Data.putObject");

console.log("obj = " + obj);
//console.log("typeof obj = " + (typeof obj));
//console.log("obj prototype = " + Object.prototype.toString.call(obj));

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


