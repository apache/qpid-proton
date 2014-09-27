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
 * The global variable PROTON_TOTAL_STACK may be used in a similar way to increase
 * the stack size from its default of 5*1024*1024 = 5242880. It is worth noting
 * that Strings are allocated on the stack, so you may need this if you end up
 * wanting to send very large strings.
 * @namespace proton
 */
var Module = {};

// If the global variable PROTON_TOTAL_MEMORY has been set by the application this
// will result in the emscripten heap getting set to the next multiple of
// 16777216 above PROTON_TOTAL_MEMORY.
if (typeof process === 'object' && typeof require === 'function') {
    if (global['PROTON_TOTAL_MEMORY']) {
        Module['TOTAL_MEMORY'] = global['PROTON_TOTAL_MEMORY'];
    }
    if (global['PROTON_TOTAL_STACK']) {
        Module['TOTAL_STACK'] = global['PROTON_TOTAL_STACK'];
    }
} else if (typeof window === 'object') {
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

