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
 * be publicly visible from this package/module.
 * <p>
 * Internally the binding code uses the associative array form for declaring
 * exported properties to prevent the Closure compiler from minifying e.g.
 * <pre>Module['Messenger'] = ...</pre>
 * Exported Objects can be used in client code using a more convenient namespace, e.g.:
 * <pre>
 * proton = require('qpid-proton');
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
/*                               EventDispatch                               */
/*                                                                           */
/*****************************************************************************/

/**
 * EventDispatch is a Singleton class that allows callbacks to be registered which
 * will get triggered by the emscripten WebSocket network callbacks. Clients of
 * Messenger will register callbacks by calling:
 * <pre>
 * messenger.on('work', &lt;callback function&gt;);
 * </pre>
 * EventDispatch supports callback registration from multiple Messenger instances.
 * The client callbacks will actually be called when a given messenger has work
 * available or a WebSocket close has been occurred (in which case all registered
 * callbacks will be called).
 * <p>
 * The approach implemented here allows the registered callbacks to follow a
 * similar pattern to _process_incoming and _process_outgoing in async.py
 * @memberof proton
 */
Module.EventDispatch = new function() { // Note the use of new to create a Singleton.
    var _firstCall = true; // Flag used to check the first time registerMessenger is called.
    var _messengers = {};

    /**
     * Provides functionality roughly equivalent to the following C code:
     * while (1) {
     *     pn_messenger_work(messenger, -1); // Block indefinitely until there has been socket activity.
     *     process();
     * }
     * The blocking call isn't viable in JavaScript as it is entirely asynchronous
     * and we wouldn't want to replace the while(1) with a timed loop either!!
     * This method gets triggered asynchronously by the emscripten socket events and
     * we then perform an equivalent loop for each messenger, triggering every
     * registered callback whilst there is work remaining. If triggered by close
     * we bypass the _pn_messenger_work test as it will never succeed after closing.
     */
    var _pump = function(fd, closing) {
        for (var i in _messengers) {
            if (_messengers.hasOwnProperty(i)) {
                var messenger = _messengers[i];

                if (closing) {
                    messenger._emit('work');
                } else {
                    while (_pn_messenger_work(messenger._messenger, 0) >= 0) {
                        messenger._checkSubscriptions();
                        messenger._checkErrors = false; // TODO improve error handling mechanism.
                        messenger._emit('work');
                    }
                }
            }
        }
    };

    /**
     * Listener for the emscripten socket close event. Delegates to _pump()
     * passing a flag to indicate that the socket is closing.
     */
    var _close = function(fd) {
        _pump(fd, true);
    };

    /**
     * Register the specified Messenger as being interested in network events.
     */
    this.registerMessenger = function(messenger) {
        if (_firstCall) {
            /**
             * Initialises the emscripten network callback functions. This needs
             * to be done the first time we call registerMessenger rather than
             * when we create the Singleton because emscripten's socket filesystem
             * has to be mounted before can listen for any of these events.
             */
            Module['websocket']['on']('open', _pump);
            Module['websocket']['on']('connection', _pump);
            Module['websocket']['on']('message', _pump);
            Module['websocket']['on']('close', _close);
            _firstCall = false;
        }

        var name = messenger.getName();
        _messengers[name] = messenger; 
    };

    /**
     * Unregister the specified Messenger from interest in network events.
     */
    this.unregisterMessenger = function(messenger) {
        var name = messenger.getName();
        delete _messengers[name];
    };
};

