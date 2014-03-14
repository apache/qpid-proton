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
 * This file provides a JavaScript wrapper around Proton Messenger.
 * It will be used to wrap the emscripten compiled proton-c code and minified by
 * the Closure compiler, so all comments will be stripped from the actual library.
 *
 * This JavaScript wrapper provides a slightly more object oriented interface which
 * abstracts some of the  emscripten based implementation details from client code
 * via the use of a higher level Buffer class.
 */
(function() { // Start of self-calling lambda used to wrap library and avoid polluting global namespace.

var Module = {
    // Prevents the runtime exiting as otherwise things like printf won't work from methods called by JavaScript.
    "noExitRuntime" : true
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


// N.B. Using associative array form for declaring exported properties to prevent Closure compiler minifying them.

/**
 * This class is a wrapper for Messenger's subscriptions.
 */
var Subscription = function(subscription) {
    var _public = {};
    var _subscription = subscription;

    // **************************************** Public methods ******************************************
    /**
     * TODO Not sure exactly what this does.
     * @return the Subscription's Context.
     */
    _public["getContext"] = function() {
        return _pn_subscription_get_context(_subscription);
    };

    /**
     * TODO Not sure exactly what this does.
     * @param context the Subscription's new Context.
     */
    _public["setContext"] = function(context) {
        _pn_subscription_set_context(_subscription, context);
    };

    /**
     * @return the Subscription's Address.
     */
    _public["getAddress"] = function() {
        return Pointer_stringify(_pn_subscription_address(_subscription));
    };

    return _public;
};

Module["Messenger"] = function(name) {
    var _public = {};

    var PN_EOS = -1;
    var PN_ERR = -2;
    var PN_OVERFLOW = -3;
    var PN_UNDERFLOW = -4;
    var PN_STATE_ERR = -5;
    var PN_ARG_ERR = -6;
    var PN_TIMEOUT = -7;
    var PN_INTR = -8;
    var PN_INPROGRESS = -9;

    var PN_CUMULATIVE = 0x1;

    // ALLOC_STACK will increase the stack and place the item there. When the stack is next restored (like when
    // the current function call exits), that memory will be automatically freed. The _pn_messenger constructor
    // copies the char* passed to it so there are no worries with it being freed when this constructor returns.
    var _messenger = _pn_messenger(name ? allocate(intArrayFromString(name), 'i8', ALLOC_STACK) : 0);

    // Initiate Messenger non-blocking mode. For JavaScript we make this the default behaviour and don't export
    // this method because JavaScript is fundamentally an asynchronous non-blocking execution environment.
    _pn_messenger_set_blocking(_messenger, false);

/* TODO just test crap - delete me!!
    _test(0);
    _test(allocate(intArrayFromString("Monkey"), 'i8', ALLOC_STACK));
*/


    //message = pn_message();


    // *************************************** Private methods ******************************************

    /**
     * This helper method checks the supplied error code, converts it into an
     * exception and throws the exception. This method will try to use the message
     * populated in pn_messenger_error(), if present, but if not it will fall
     * back to using the basic error code rendering from pn_code().
     * @param code the error code to check.
     */
    function _check(code) {
        if (code < 0) {
            if (code === PN_INPROGRESS) {
                return code;
            }

            var errno = _public.getErrno();
            var message = errno ? _public.getError() : Pointer_stringify(_pn_code(code));

            throw { // TODO Improve name and level.
                name:     "System Error", 
                level:    "Show Stopper", 
                message:  message, 
                toString: function() {return this.name + ": " + this.message} 
            }
        } else {
            return code;
        }
    };

    // **************************************** Public methods ******************************************

    /**
     * N.B. The following methods are not exported by the JavaScript Messenger binding for reasons described below.
     *
     * For these methods it is expected that security would be implemented via a secure WebSocket.
     * TODO what happens if we decide to implement TCP sockets via Node.js net library.
     * If we do that we may want to compile OpenSSL using emscripten and include these methods.
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
    _public["getName"] = function() {
        return Pointer_stringify(_pn_messenger_name(_messenger));
    };

    /**
     * Retrieves the timeout for a Messenger.
     * @return zero because JavaScript is fundamentally non-blocking.
     */
    _public["getTimeout"] = function() {
        return 0;
    };

    /**
     * Accessor for messenger blocking mode.
     * @return false because JavaScript is fundamentally non-blocking.
     */
    _public["isBlocking"] = function() {
        return true;
    };

    /**
     * Free the Messenger. This will close all connections that are managed
     * by the Messenger. Call the stop method before destroying the Messenger.
     * N.B. This method has to be called explicitly in JavaScript as we can't
     * intercept finalisers so we need to remember to free before removing refs.
     */
    _public["free"] = function() {
        _pn_messenger_free(_messenger);
    };

    /**
     * @return the most recent error message code.
     */
    _public["getErrno"] = function() {
        return _pn_messenger_errno(_messenger);
    };

    /**
     * @return the most recent error message as a String.
     */
    _public["getError"] = function() {
        return Pointer_stringify(_pn_error_text(_pn_messenger_error(_messenger)));
    };

    /**
     * Returns the size of the outgoing window that was set with
     * pn_messenger_set_outgoing_window. The default is 0.
     * @return the outgoing window.
     */
    _public["getOutgoingWindow"] = function() {
        return _pn_messenger_get_outgoing_window(_messenger);
    };

    /**
     * Sets the outgoing tracking window for the Messenger. The Messenger will
     * track the remote status of this many outgoing deliveries after calling
     * send. Defaults to zero.
     *
     * A Message enters this window when you call the put() method with the Message.
     * If your outgoing window size is n, and you call put n+1 times, status
     * information will no longer be available for the first Message.
     * @param window the size of the tracking window in messages.
     */
    _public["setOutgoingWindow"] = function(window) {
        _check(_pn_messenger_set_outgoing_window(_messenger, window));
    };

    /**
     * Returns the size of the incoming window that was set with
     * pn_messenger_set_incoming_window. The default is 0.
     * @return the incoming window.
     */
    _public["getIncomingWindow"] = function() {
        return _pn_messenger_get_incoming_window(_messenger);
    };

    /**
     * Sets the incoming tracking window for the Messenger. The Messenger will
     * track the remote status of this many incoming deliveries after calling
     * send. Defaults to zero.
     *
     * Messages enter this window only when you take them into your application
     * using get. If your incoming window size is n, and you get n+1 messages
     * without explicitly accepting or rejecting the oldest message, then the
     * Message that passes beyond the edge of the incoming window will be assigned
     * the default disposition of its link.
     * @param window the size of the tracking window in messages.
     */
    _public["setIncomingWindow"] = function(window) {
        _check(_pn_messenger_set_incoming_window(_messenger, window));
    };

    /**
     * Currently a no-op placeholder. For future compatibility, do not send or
     * recv messages before starting the Messenger.
     */
    _public["start"] = function() {
        _check(_pn_messenger_start(_messenger));
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
    _public["stop"] = function() {
        return _check(_pn_messenger_stop(_messenger));
    };

    /**
     * @return Returns true iff a Messenger is in the stopped state.
     */
    _public["isStopped"] = function() {
        return (_pn_messenger_stopped(_messenger) > 0);
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
    _public["subscribe"] = function(source) {
console.log("subscribe: haven't yet proved this works yet");
        if (!source) {
            _check(-6); // PN_ARG_ERR
        }
        var subscription = _pn_messenger_subscribe(_messenger,
                                                   allocate(intArrayFromString(source), 'i8', ALLOC_STACK));
        if (!subscription) {
            _check(-2); // PN_ERR
        }
        return Subscription(subscription);
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
    _public["put"] = function(message) {
console.log("put: not fully implemented yet");
        // TODO message._pre_encode()
        _check(_pn_messenger_put(_messenger, message));
        return _pn_messenger_outgoing_tracker(_messenger);
    };

    /**
     * Gets the last known remote state of the delivery associated with the given tracker.
     * @param tracker the tracker whose status is to be retrieved.
     * @return one of None, PENDING, REJECTED, or ACCEPTED.
     */
    _public["status"] = function(tracker) {
console.log("status: not fully implemented yet");
        var disp = _pn_messenger_status(_messenger, tracker);
        return disp; // TODO return a more friendly status.
    };

    /**
     * Checks if the delivery associated with the given tracker is still waiting to be sent.
     * @param tracker the tracker identifying the delivery.
     * @return true if delivery is still buffered.
     */
    _public["isBuffered"] = function(tracker) {
        return (_pn_messenger_buffered(_messenger, tracker) > 0);
    };

    /**
     * Frees a Messenger from tracking the status associated with a given tracker.
     * If you don't supply a tracker, all outgoing messages up to the most recent
     * will be settled.
     * @param tracker the tracker identifying the delivery.
     */
    _public["settle"] = function(tracker) {
console.log("settle: not fully tested yet");
        var flags = 0;
        if (!tracker) {
            tracker = _pn_messenger_outgoing_tracker(_messenger);
            flags = PN_CUMULATIVE;
        }

        _check(_pn_messenger_settle(_messenger, tracker, flags));
    };

    /**
     * Sends or receives any outstanding messages queued for a Messenger.
     * For JavaScript the only timeout that makes sense is 0 == do not block.
     * This method may also do I/O work other than sending and receiving messages.
     * For example, closing connections after messenger.stop() has been called.
     * @return 0 if no work to do, < 0 if error, or 1 if work was done.
     */
    _public["work"] = function() {
        //return _pn_messenger_work(_messenger, timeout);
        return _pn_messenger_work(_messenger, 0);
    };

    /**
     * Receives up to limit messages into the incoming queue.  If no value for limit
     * is supplied, this call will receive as many messages as it can buffer internally.
     * @param limit the maximum number of messages to receive or -1 to to receive
     *        as many messages as it can buffer internally.
     */
    _public["recv"] = function(limit) {
        _check(_pn_messenger_recv(_messenger, (limit ? limit : -1)));
    };

    /**
     * Returns the capacity of the incoming message queue of messenger. Note this
     * count does not include those messages already available on the incoming queue.
     * @return the message queue capacity.
     */
    _public["receiving"] = function() {
        return _pn_messenger_receiving(_messenger);
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
    _public["get"] = function(message) {
console.log("get: not fully implemented yet");
/*
    if message is None:
      impl = None
    else:
      impl = message._msg
*/

        _check(_pn_messenger_get(_messenger, message));
/*
    if message is not None:
      message._post_decode()
*/
        // TODO message._post_decode()

        return _pn_messenger_incoming_tracker(_messenger);
    };

    /**
     * Returns the Subscription of the Message returned by the most recent call
     * to get, or null if pn_messenger_get has not yet been called.
     * @return a Subscription or null if get has never been called for this Messenger.
     */
    _public["incomingSubscription"] = function() {
console.log("incomingSubscription: haven't yet proved this works yet");

        var subscription = _pn_messenger_incoming_subscription(_messenger);
        if (subscription) {
            return Subscription(subscription);
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
    _public["accept"] = function(tracker) {
console.log("accept: not fully tested yet");
        var flags = 0;
        if (!tracker) {
            tracker = _pn_messenger_incoming_tracker(_messenger);
            flags = PN_CUMULATIVE;
        }

        _check(_pn_messenger_accept(_messenger, tracker, flags));
    };

    /**
     * Rejects the Message indicated by the tracker.  If no tracker is supplied,
     * all messages that have been returned by the get method are rejected, except
     * those already auto-settled by passing beyond your outgoing window size.
     * @param tracker the tracker identifying the delivery.
     */
    _public["reject"] = function(tracker) {
console.log("reject: not fully tested yet");
        var flags = 0;
        if (!tracker) {
            tracker = _pn_messenger_incoming_tracker(_messenger);
            flags = PN_CUMULATIVE;
        }

        _check(_pn_messenger_reject(_messenger, tracker, flags));
    };

    /**
     * Returns the number of messages in the outgoing message queue of a messenger.
     * @return the outgoing queue depth.
     */
    _public["outgoing"] = function() {
        return _pn_messenger_outgoing(_messenger);
    };

    /**
     * Returns the number of messages in the incoming message queue of a messenger.
     * @return the incoming queue depth.
     */
    _public["incoming"] = function() {
        return _pn_messenger_incoming(_messenger);
    };






    /**
     * 
     * @param pattern a glob pattern to select messages.
     * @param address an address indicating outgoing address rewrite.
     */
    _public["route"] = function(pattern, address) {
console.log("route: not fully tested yet");

        _check(_pn_messenger_route(_messenger,
                                   allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                   allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
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
    _public["rewrite"] = function(pattern, address) {
console.log("rewrite: not fully tested yet");

        _check(_pn_messenger_rewrite(_messenger,
                                     allocate(intArrayFromString(pattern), 'i8', ALLOC_STACK),
                                     allocate(intArrayFromString(address), 'i8', ALLOC_STACK)));
    };



    return _public;
};

/*
Module["Inflate"] = function(size) {
    var _public = {};
    var stream = _inflateInitialise();
    var inputBuffer  = Buffer(size);
    var outputBuffer = Buffer(size);

    // Public methods
    _public["destroy"] = function() {
        _inflateDestroy(stream);
        inputBuffer.destroy();
        outputBuffer.destroy();
    };

    _public["reset"] = function() {
        _inflateReset(stream);
    };

    _public["inflate"] = function(ptr) {
        ptr = ptr ? ptr : outputBuffer.getRaw();
        var inflatedSize; // Pass by reference variable - need to use Module.setValue to initialise it.
        setValue(inflatedSize, outputBuffer.size, "i32");
        var err = _zinflate(stream, ptr, inflatedSize, inputBuffer.getRaw(), inputBuffer.size);
        inflatedSize = getValue(inflatedSize, "i32"); // Dereference the real inflatedSize value;
        outputBuffer.setSize(inflatedSize);
        return ((err < 0) ? err : inflatedSize); // Return the inflated size, or error code if inflation fails.
    };

    // Export methods from the input and output buffers for use by client code.
    _public["setInputBufferSize"] = inputBuffer.setSize;
    _public["getRawInputBuffer"] = inputBuffer.getRaw;
    _public["getInputBuffer"] = inputBuffer.getBuffer;

    _public["setOutputBufferSize"] = outputBuffer.setSize;
    _public["getRawOutBuffer"] = outputBuffer.getRaw;
    _public["getOutputBuffer"] = outputBuffer.getBuffer;

    return _public;
};
*/


