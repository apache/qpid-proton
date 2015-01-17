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
 * This file defines the Module Object which provides a namespace around the Proton
 * Messenger API. The Module object is used extensively by the emscripten runtime,
 * however for convenience it is exported with the name "proton" and not "Module".
 * <p>
 * The emscripten compiled proton-c code and the JavaScript binding code will be 
 * minified by the Closure compiler, so all comments will be stripped from the
 * actual library.
 * <p>
 * This JavaScript wrapper provides a somewhat more idiomatic object oriented
 * interface which abstracts the low-level emscripten based implementation details
 * from client code. Any similarities to the Proton Python binding are deliberate.
 * @file
 */

/**
 * The Module Object is exported by emscripten for all execution platforms, we
 * use it as a namespace to allow us to selectively export only what we wish to
 * be publicly visible from this package/module, which is wrapped in a closure.
 * <p>
 * Internally the binding code uses the associative array form for declaring
 * exported properties to prevent the Closure compiler from minifying e.g.
 * <pre>Module['Messenger'] = ...</pre>
 * Exported Objects can however be used in client code using a more convenient
 * and obvious proton namespace, e.g.:
 * <pre>
 * var proton = require('qpid-proton');
 * var messenger = new proton.Messenger();
 * var message = new proton.Message();
 * ...
 * </pre>
 * The core part of this library is actually proton-c compiled into JavaScript.
 * In order to provide C style memory management (malloc/free) emscripten uses
 * a "virtual heap", which is actually a pre-allocated ArrayBuffer. The size of
 * this virtual heap is set as part of the runtime initialisation and cannot be
 * changed subsequently (the default size is 16*1024*1024 = 16777216).
 * <p>
 * Applications can specify the size of virtual heap that they require via the
 * global variable PROTON_TOTAL_MEMORY, this must be set <b>before</b> the library is
 * loaded e.g. in Node.js an application would do:
 * <pre>
 * PROTON_TOTAL_MEMORY = 50000000; // Note no var - it needs to be global.
 * var proton = require('qpid-proton');
 * ...
 * </pre>
 * A browser based application would do:
 * <pre>
 * &lt;script type="text/javascript"&gt;PROTON_TOTAL_MEMORY = 50000000;&lt;/script&gt;
 * &lt;script type="text/javascript" src="proton.js">&lt;/script&gt;
 * </pre>
 * If the global variable PROTON_TOTAL_MEMORY has been set by the application this
 * will result in the emscripten heap getting set to the next multiple of
 * 16777216 above PROTON_TOTAL_MEMORY.
 * <p>
 * The global variable PROTON_TOTAL_STACK may be used in a similar way to increase
 * the stack size from its default of 5*1024*1024 = 5242880. It is worth noting
 * that Strings are allocated on the stack, so you may need this if you end up
 * wanting to send very large strings.
 * @namespace proton
 */
var Module = {};

if (typeof global === 'object') { // If Node.js
    if (global['PROTON_TOTAL_MEMORY']) {
        Module['TOTAL_MEMORY'] = global['PROTON_TOTAL_MEMORY'];
    }
    if (global['PROTON_TOTAL_STACK']) {
        Module['TOTAL_STACK'] = global['PROTON_TOTAL_STACK'];
    }
} else if (typeof window === 'object') { // If Browser
    if (window['PROTON_TOTAL_MEMORY']) {
        Module['TOTAL_MEMORY'] = window['PROTON_TOTAL_MEMORY'];
    }
    if (window['PROTON_TOTAL_STACK']) {
        Module['TOTAL_STACK'] = window['PROTON_TOTAL_STACK'];
    }
}

/*****************************************************************************/
/*                                                                           */
/*                               EventDispatch                               */
/*                                                                           */
/*****************************************************************************/

/**
 * EventDispatch is a Singleton class that allows callbacks to be registered,
 * which will get triggered by the emscripten WebSocket network callbacks.
 * Clients of Messenger will register callbacks by calling:
 * <pre>
 * messenger.on('error', &lt;callback function&gt;);
 * messenger.on('work', &lt;callback function&gt;);
 * messenger.on('subscription', &lt;callback function&gt;);
 * </pre>
 * EventDispatch supports callback registration from multiple Messenger instances.
 * The client callbacks will actually be called when a given messenger has work
 * available or a WebSocket close has been occurred.
 * <p>
 * The approach implemented here allows the registered callbacks to follow a
 * similar pattern to _process_incoming and _process_outgoing in async.py
 * @constructor proton.EventDispatch
 */
Module.EventDispatch = new function() { // Note the use of new to create a Singleton.
    var POLLIN  = 0x001;
    var POLLOUT = 0x004;
    var _error = null;
    var _messengers = {};  // Keyed by name.
    var _selectables = {}; // Keyed by file descriptor.

    var _initialise = function() {
        /**
         * Initialises the emscripten network callback functions. This needs
         * to be done the first time we call registerMessenger rather than
         * when we create the Singleton because emscripten's socket filesystem
         * has to be mounted before can listen for any of these events.
         */
        Module['websocket']['on']('open', _pump);
        Module['websocket']['on']('message', _pump);
        Module['websocket']['on']('connection', _connectionHandler);
        Module['websocket']['on']('close', _closeHandler);
        Module['websocket']['on']('error', _errorHandler);

        /**
         * For Node.js the network code uses the ws WebSocket library, see
         * https://github.com/einaros/ws. The following is a "Monkey Patch"
         * that fixes a problem with Receiver.js where it wasn't checking if
         * an Object was null before accessing its properties, so it was
         * possible to see errors like:
         * TypeError: Cannot read property 'fragmentedOperation' of null
         * at Receiver.endPacket (.....node_modules/ws/lib/Receiver.js:224:18)
         * This problem is generally seen in Server code after messenger.stop()
         * I *think* that the underlying issue is actually because ws calls
         * cleanup directly rather than pushing it onto the event loop so the
         * this.state stuff gets cleared before the endPacket method is called.
         * This fix simply interposes a check to avoid calling endPacket if
         * the state has been cleared (i.e. the WebSocket has been closed).
         */
        if (ENVIRONMENT_IS_NODE) {
            try {
                var ws = require('ws');
                // Array notation to stop Closure compiler minifying properties we need.
                ws['Receiver'].prototype['originalEndPacket'] = ws['Receiver'].prototype['endPacket'];
                ws['Receiver'].prototype['endPacket'] = function() {
                    if (this['state']) {
                        this['originalEndPacket']();
                    }
                };
            } catch (e) {
                console.error("Failed to apply Monkey Patch to ws WebSocket library");
            }
        }

        _initialise = function() {}; // After first call replace with null function.
    };

    /**
     * Messenger error handling can be a bit inconsistent and in several places
     * rather than returning an error code or setting an error it simply writes
     * to fprintf. This is something of a Monkey Patch that replaces the emscripten
     * library fprintf call with one that checks the message and sets a variable
     * if the message is an ERROR. TODO At some point hopefully Dominic Evans'
     * patch on Jira PROTON-571 will render this code redundant.
     */
    _fprintf = function(stream, format, varargs) {
        var array = __formatString(format, varargs);
        array.pop(); // Remove the trailing \n
        var string = intArrayToString(array); // Convert to native JavaScript string.
        if (string.indexOf('ERROR') === -1) { // If not an ERROR just log the message.
            console.log(string);
        } else {
            _error = string;
        }
    };

    /**
     * This method uses some low-level emscripten internals (stream = FS.getStream(fd),
     * sock = stream.node.sock, peer = SOCKFS.websocket_sock_ops.getPeer) to find
     * the underlying WebSocket associated with a given file descriptor value.
     */
    var _getWebSocket = function(fd) {
        var stream = FS.getStream(fd);
        if (stream) {
            var sock = stream.node.sock;
            if (sock.server) {
                return sock.server;
            }
            var peer = SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport);
            if (peer) {
                return peer.socket;
            }
        }
        return null;
    };

    /**
     * This method iterates through all registered Messengers and retrieves any
     * pending selectables, which are stored in a _selectables map keyed by fd.
     */
    var _updateSelectables = function() {
        var sel = 0;
        var fd = -1;
        for (var name in _messengers) {
            var messenger = _messengers[name];
            while ((sel = _pn_messenger_selectable(messenger._messenger))) {
                fd = _pn_selectable_get_fd(sel);
                // Only register valid selectables, otherwise free them.
                if (fd === -1) {
                    _pn_selectable_free(sel);
                } else {
                    _selectables[fd] = {messenger: messenger, selectable: sel, socket: _getWebSocket(fd)};
                }
            }
        }
        return fd; // Return the most recently added selector's file descriptor.
    };

    /**
     * Continually pump data while there's still work to do.
     */
    var _pump = function(fd) {
        while (_pumpOnce(fd));
    };

    /**
     * This method more or less follows the pattern of the pump_once method from
     * class Pump in tests/python/proton_tests/messenger.py. It looks a little
     * different because the select/poll implemented here uses some low-level
     * emscripten internals (stream = FS.getStream(fd), sock = stream.node.sock,
     * mask = sock.sock_ops.poll(sock)). We use the internals so we don't have
     * to massage from file descriptors into the C style poll interface.
     */
    var _pumpOnce = function(fdin) {
        _updateSelectables();

        var work = false;
        for (var fd in _selectables) {
            var selectable = _selectables[fd];
            if (selectable.socket) {
                var messenger = selectable.messenger;
                var sel = selectable.selectable;
                var terminal = _pn_selectable_is_terminal(sel);
                if (terminal) {
//console.log(fd + " is terminal");
                    _closeHandler(fd);
                } else if (!fdin || (fd == fdin)) {
                    var stream = FS.getStream(fd);
                    if (stream) {
                        var sock = stream.node.sock;
                        if (sock.sock_ops.poll) {
                            var mask = sock.sock_ops.poll(sock); // Low-level poll call.
                            if (mask) {
                                var capacity = _pn_selectable_is_reading(sel);
                                var pending = _pn_selectable_is_writing(sel);

                                if ((mask & POLLIN) && capacity) {
//console.log("- readable fd = " + fd + ", capacity = " + _pn_selectable_capacity(sel));
                                    _error = null; // May get set by _pn_selectable_readable.
                                    _pn_selectable_readable(sel);
                                    work = true;
                                }
                                if ((mask & POLLOUT) && pending) {
//console.log("- writable fd = " + fd + ", pending = " + _pn_selectable_pending(sel));
                                    _pn_selectable_writable(sel);
                                    work = true;
                                }

                                var errno = messenger['getErrno']();
                                _error = errno ? messenger['getError']() : _error;
                                if (_error) {
                                    _errorHandler([fd, 0, _error]);
                                } else {
                                    // Don't send work Event if it's a listen socket.
                                    if (work && !sock.server) {
                                        messenger._checkSubscriptions();
                                        messenger._emit('work');
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return work;
    };

    /**
     * Handler for the emscripten socket connection event.
     * The causes _pump to be called with no fd, forcing all fds to be checked.
     */
    var _connectionHandler = function(fd) {
        _pump();
    };

    /**
     * Handler for the emscripten socket close event.
     */
    var _closeHandler = function(fd) {
        _updateSelectables();

        var selectable = _selectables[fd];
        if (selectable && selectable.socket) {
            selectable.socket = null;
//console.log("_closeHandler fd = " + fd);

            /**
             * We use the timeout to ensure that execution of the function to
             * actually free and remove the selectable is deferred until next
             * time round the (internal JavaScript) event loop. This turned out
             * to be necessary because in some cases the ws WebSocket library
             * calls the onclose callback (concurrently!!) before the onmessage
             * callback exits, which could result in _pn_selectable_free being
             * called whilst _pn_selectable_writable is executing, which is bad!!
             */
            setTimeout(function() {
//console.log("deferred _closeHandler fd = " + fd);
                // Close and remove the selectable.
                var sel = selectable.selectable;
                _pn_selectable_free(sel); // This closes the underlying socket too.
                delete _selectables[fd];

                var messenger = selectable.messenger;
                messenger._emit('work');
            }, 0);
        }
    };

    /**
     * Handler for the emscripten socket error event.
     */
    var _errorHandler = function(error) {
        var fd = error[0];
        var message = error[2];

        _updateSelectables();

        var selectable = _selectables[fd];
        if (selectable) {
            // Close and remove the selectable.
            var sel = selectable.selectable;
            _pn_selectable_free(sel); // This closes the underlying socket too.
            delete _selectables[fd];

            var messenger = selectable.messenger;

            // Remove any pending Subscriptions whose fd matches the error fd.
            var subscriptions = messenger._pendingSubscriptions;
            for (var i = 0; i < subscriptions.length; i++) {
                subscription = subscriptions[i];
                // Use == not === as fd is a number and subscription.fd is a string.
                if (subscription.fd == fd) {
                    messenger._pendingSubscriptions.splice(i, 1);
                    if (message.indexOf('EHOSTUNREACH:') === 0) {
                        message = 'CONNECTION ERROR (' + subscription.source + '): bind: Address already in use';
                    }
                    messenger._emit('error', new Module['SubscriptionError'](subscription.source, message));
                    return;
                }
            }

            messenger._emit('error', new Module['MessengerError'](message));
        }
    };

    /**
     * Flush any data that has been written by the Messenger put() method.
     * @method pump
     * @memberof! proton.EventDispatch#
     */
    this.pump = function() {
        _pump();
    };

    /**
     * For a given Messenger instance retrieve the bufferedAmount property from
     * any connected WebSockets and return the aggregate total sum.
     * @method getBufferedAmount
     * @memberof! proton.EventDispatch#
     * @param {proton.Messenger} messenger the Messenger instance that we want
     *        to find the total buffered amount for.
     * @returns {number} the total sum of the bufferedAmount property across all
     *          connected WebSockets.
     */
    this.getBufferedAmount = function(messenger) {
        var total = 0;
        for (var fd in _selectables) {
            var selectable = _selectables[fd];
            if (selectable.messenger === messenger && selectable.socket) {
                total += selectable.socket.bufferedAmount | 0; 
            }
        }
        return total;
    };

    /**
     * Subscribe to a specified source address.
     * <p>
     * This method is delegated to by the subscribe method of {@link proton.Messenger}.
     * We delegate to EventDispatch because we create Subscription objects that
     * contain some additional information (such as file descriptors) which are
     * only available to EventDispatch and we don't really want to expose to the
     * wider API. This low-level information is mainly used for error handling
     * which is itself encapsulated in EventDispatch.
     * @method subscribe
     * @memberof! proton.EventDispatch#
     * @param {proton.Messenger} messenger the Messenger instance that this
     *        subscription relates to.
     * @param {string} source the address that we'd like to subscribe to.
     */
    this.subscribe = function(messenger, source) {
        // First update selectables before subscribing so we can work out the
        // Subscription fd (which will be the listen file descriptor).
        _updateSelectables();
        var sp = Runtime.stackSave();
        var subscription = _pn_messenger_subscribe(messenger._messenger,
                                                   allocate(intArrayFromString(source), 'i8', ALLOC_STACK));
        Runtime.stackRestore(sp);
        var fd = _updateSelectables();

        subscription = new Subscription(subscription, source, fd);
        messenger._pendingSubscriptions.push(subscription);

        // For passive subscriptions emit a subscription event (almost) immediately,
        // otherwise defer until the address has been resolved remotely.
        if (subscription.passive) {
            // We briefly delay the call to checkSubscriptions because it is possible
            // for passive subscriptions to fail if another process is bound to the
            // port specified in the subscription.
            var check = function() {messenger._checkSubscriptions();};
            setTimeout(check, 10);
        }

        return subscription;
    };

    /**
     * Register the specified Messenger as being interested in network events.
     * @method registerMessenger
     * @memberof! proton.EventDispatch#
     * @param {proton.Messenger} messenger the Messenger instance we want to
     *        register to receive network events.
     */
    this.registerMessenger = function(messenger) {
        _initialise();

        var name = messenger['getName']();
        _messengers[name] = messenger;
    };

    /**
     * Unregister the specified Messenger from interest in network events.
     * @method unregisterMessenger
     * @memberof! proton.EventDispatch#
     * @param {proton.Messenger} messenger the Messenger instance we want to
     *        unregister from receiving network events.
     */
    this.unregisterMessenger = function(messenger) {
        var name = messenger['getName']();
        delete _messengers[name];
    };
};

