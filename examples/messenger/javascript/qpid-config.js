#!/usr/bin/env node
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
 * Port of qpid-config to JavaScript for node.js, mainly intended as a demo to
 * illustrate using QMF2 in JavaScript using the proton.Messenger JS binding.
 * It illustrates a few things including how to use Messenger completely
 * asynchronously including using an async request/response pattern with
 * correlation IDs. It also proves interoperability of AMQP Map, List etc.
 * between C++ and JavaScript as QMF2 is pretty much all about Lists of Maps.
 * <p>
 * The actual QMF2 code is pretty simple as we're just doing a basic getObjects
 * it's made all the simpler because we can use JavaScript object literals as
 * the JavaScript binding serialises and deserialises directly between JavaScript
 * Objects and Lists and the AMQP type system so something that can be quite
 * involved in languages like C++ and Java becomes quite simple in JavaScript,
 * though the asynchronous nature of JavaScript provides its own opportunities
 * for complication best illustrated by the need for the correlator object.
 */

// Check if the environment is Node.js and if so import the required library.
if (typeof exports !== "undefined" && exports !== null) {
    proton = require("qpid-proton");
}

var addr = 'guest:guest@localhost:5673';
//var addr = 'localhost:5673';
var address = 'amqp://' + addr + '/qmf.default.direct';
console.log(address);

var replyTo = '';
var subscription;
var subscribed = false;

var message = new proton.Message();
var messenger = new proton.Messenger();

/**
 * The correlator object is a mechanism used to correlate requests with their
 * aynchronous responses. It might possible be better to make use of Promises
 * to implement part of this behaviour but a mechanism would still be needed to
 * correlate a request with its response callback in order to wrap things up in
 * a Promise, so much of the behaviour of this object would still be required.
 * In addition it seemed to make sense to make this QMF2 implementation fairly
 * free of dependencies and using Promises would require external libraries.
 * Instead the correlator implements "Promise-like" semantics, you might call it
 * a broken Promise :-)
 * <p>
 * in particular the request method behaves a *bit* like Promise.all() though it
 * is mostly fake and takes an array of functions that call the add() method
 * which is really the method used to associate response objects by correlationID.
 * The then method is used to register a listener that will be called when all
 * the requests that have been registered have received responses.
 * TODO error/timeout handling.
 */
var correlator = {
    _resolve: null,
    _objects: {},
    add: function(id) {
        this._objects[id] = {complete: false, list: null};
    },
    request: function() {
        this._resolve = function() {console.log("Warning: No resolver has been set")};
        return this;
    },
    then: function(resolver) {
        this._resolve = resolver ? resolver : this._resolve;
    },
    resolve: function() {
        var opcode = message.properties['qmf.opcode'];
        var correlationID = message.getCorrelationID();
        var resp = this._objects[correlationID];
        if (opcode === '_query_response') {
            if (resp.list) {
                Array.prototype.push.apply(resp.list, message.body); // This is faster than concat.
            } else {
                resp.list = message.body;
            }

            var partial = message.properties['partial'];
            if (!partial) {
                resp.complete = true;
            }

            this._objects[correlationID] = resp;
            this._checkComplete();
        } else if (opcode === '_method_response' || opcode === '_exception') {
            resp.list = message.body;
            resp.complete = true;
            this._objects[correlationID] = resp;
            this._checkComplete();
        } else {
            console.error("Bad Message response, qmf.opcode = " + opcode);
        }
    },
    _checkComplete: function() {
        var response = {};
        for (var id in this._objects) {
            var object = this._objects[id];
            if (object.complete) {
                response[id] = object.list;
            } else {
                return;
            }
        }

        this._objects = {}; // Clear state ready for next call.
        this._resolve(response.method ? response.method : response);
    }
};

var pumpData = function() {
    if (!subscribed) {
        var subscriptionAddress = subscription.getAddress();
        if (subscriptionAddress) {
            subscribed = true;
            var splitAddress = subscriptionAddress.split('/');
            replyTo = splitAddress[splitAddress.length - 1];

            onSubscription();
        }
    }

    while (messenger.incoming()) {
        // The second parameter forces Binary payloads to be decoded as strings
        // this is useful because the broker QMF Agent encodes strings as AMQP
        // binary, which is a right pain from an interoperability perspective.
        var t = messenger.get(message, true);
        correlator.resolve();
        messenger.accept(t);
    }

    if (messenger.isStopped()) {
        message.free();
        messenger.free();
    }
};

var getObjects = function(packageName, className) {
    message.setAddress(address);
    message.setSubject('broker');
    message.setReplyTo(replyTo);
    message.setCorrelationID(className);
    message.properties = {
        "routing-key": "broker", // Added for Java Broker
        "x-amqp-0-10.app-id": "qmf2",
        "method": "request",
        "qmf.opcode": "_query_request",
    };
    message.body = {
        "_what": "OBJECT",
        "_schema_id": {
            "_package_name": packageName,
            "_class_name": className
        }
    };

    correlator.add(className);
    messenger.put(message);
};

var invokeMethod = function(object, method, arguments) {
    var correlationID = 'method';
    message.setAddress(address);
    message.setSubject('broker');
    message.setReplyTo(replyTo);
    message.setCorrelationID(correlationID);
    message.properties = {
        "routing-key": "broker", // Added for Java Broker
        "x-amqp-0-10.app-id": "qmf2",
        "method": "request",
        "qmf.opcode": "_method_request",
    };
    message.body = {
        "_object_id": object._object_id,
        "_method_name" : method,
        "_arguments"   : arguments
    };

    correlator.add(correlationID);
    messenger.put(message);
};

messenger.on('error', function(error) {console.log(error);});
messenger.on('work', pumpData);
messenger.setOutgoingWindow(1024);
messenger.start();

subscription = messenger.subscribe('amqp://' + addr + '/#');
messenger.recv(); // Receive as many messages as messenger can buffer.


/************************* qpid-config business logic ************************/

var _usage =
'Usage:  qpid-config [OPTIONS]\n' +
'        qpid-config [OPTIONS] exchanges [filter-string]\n' +
'        qpid-config [OPTIONS] queues    [filter-string]\n' +
'        qpid-config [OPTIONS] add exchange <type> <name> [AddExchangeOptions]\n' +
'        qpid-config [OPTIONS] del exchange <name>\n' +
'        qpid-config [OPTIONS] add queue <name> [AddQueueOptions]\n' +
'        qpid-config [OPTIONS] del queue <name> [DelQueueOptions]\n' +
'        qpid-config [OPTIONS] bind   <exchange-name> <queue-name> [binding-key]\n' +
'                  <for type xml>     [-f -|filename]\n' +
'                  <for type header>  [all|any] k1=v1 [, k2=v2...]\n' +
'        qpid-config [OPTIONS] unbind <exchange-name> <queue-name> [binding-key]\n' +
'        qpid-config [OPTIONS] reload-acl\n' +
'        qpid-config [OPTIONS] add <type> <name> [--argument <property-name>=<property-value>]\n' +
'        qpid-config [OPTIONS] del <type> <name>\n' +
'        qpid-config [OPTIONS] list <type> [--show-property <property-name>]\n';

var usage = function() {
    console.log(_usage);
    process.exit(-1);
};

var _description =
'Examples:\n' +
'\n' +
'$ qpid-config add queue q\n' +
'$ qpid-config add exchange direct d -a localhost:5672\n' +
'$ qpid-config exchanges -b 10.1.1.7:10000\n' +
'$ qpid-config queues -b guest/guest@broker-host:10000\n' +
'\n' +
'Add Exchange <type> values:\n' +
'\n' +
'    direct     Direct exchange for point-to-point communication\n' +
'    fanout     Fanout exchange for broadcast communication\n' +
'    topic      Topic exchange that routes messages using binding keys with wildcards\n' +
'    headers    Headers exchange that matches header fields against the binding keys\n' +
'    xml        XML Exchange - allows content filtering using an XQuery\n' +
'\n' +
'\n' +
'Queue Limit Actions:\n' +
'\n' +
'    none (default) - Use broker\'s default policy\n' +
'    reject         - Reject enqueued messages\n' +
'    ring           - Replace oldest unacquired message with new\n' +
'\n' +
'Replication levels:\n' +
'\n' +
'    none           - no replication\n' +
'    configuration  - replicate queue and exchange existence and bindings, but not messages.\n' +
'    all            - replicate configuration and messages\n';

var _options =
'Options:\n' +
'  -h, --help            show this help message and exit\n' +
'\n' +
'  General Options:\n' +
'    -t <secs>, --timeout=<secs>\n' +
'                        Maximum time to wait for broker connection (in\n' +
'                        seconds)\n' +
'    -r, --recursive     Show bindings in queue or exchange list\n' +
'    -b <address>, --broker=<address>\n' +
'                        Address of qpidd broker with syntax:\n' +
'                        [username/password@] hostname | ip-address [:<port>]\n' +
'    -a <address>, --broker-addr=<address>\n' +
/* TODO Connection options
'    --sasl-mechanism=<mech>\n' +
'                        SASL mechanism for authentication (e.g. EXTERNAL,\n' +
'                        ANONYMOUS, PLAIN, CRAM-MD5, DIGEST-MD5, GSSAPI). SASL\n' +
'                        automatically picks the most secure available\n' +
'                        mechanism - use this option to override.\n' +
'    --ssl-certificate=<cert>\n' +
'                        Client SSL certificate (PEM Format)\n' +
'    --ssl-key=<key>     Client SSL private key (PEM Format)\n' +
'    --ha-admin          Allow connection to a HA backup broker.\n' +
*/
'\n' +
'  Options for Listing Exchanges and Queues:\n' +
'    --ignore-default    Ignore the default exchange in exchange or queue list\n' +
'\n' +
'  Options for Adding Exchanges and Queues:\n' +
'    --alternate-exchange=<aexname>\n' +
'                        Name of the alternate-exchange for the new queue or\n' +
'                        exchange. Exchanges route messages to the alternate\n' +
'                        exchange if they are unable to route them elsewhere.\n' +
'                        Queues route messages to the alternate exchange if\n' +
'                        they are rejected by a subscriber or orphaned by queue\n' +
'                        deletion.\n' +
'    --durable           The new queue or exchange is durable.\n' +
'    --replicate=<level>\n' +
'                        Enable automatic replication in a HA cluster. <level>\n' +
'                        is \'none\', \'configuration\' or \'all\').\n' +
'\n' +
'  Options for Adding Queues:\n' +
'    --file-count=<n>    Number of files in queue\'s persistence journal\n' +
'    --file-size=<n>     File size in pages (64KiB/page)\n' +
'    --max-queue-size=<n>\n' +
'                        Maximum in-memory queue size as bytes\n' +
'    --max-queue-count=<n>\n' +
'                        Maximum in-memory queue size as a number of messages\n' +
'    --limit-policy=<policy>\n' +
'                        Action to take when queue limit is reached\n' +
'    --lvq-key=<key>     Last Value Queue key\n' +
'    --generate-queue-events=<n>\n' +
'                        If set to 1, every enqueue will generate an event that\n' +
'                        can be processed by registered listeners (e.g. for\n' +
'                        replication). If set to 2, events will be generated\n' +
'                        for enqueues and dequeues.\n' +
'    --flow-stop-size=<n>\n' +
'                        Turn on sender flow control when the number of queued\n' +
'                        bytes exceeds this value.\n' +
'    --flow-resume-size=<n>\n' +
'                        Turn off sender flow control when the number of queued\n' +
'                        bytes drops below this value.\n' +
'    --flow-stop-count=<n>\n' +
'                        Turn on sender flow control when the number of queued\n' +
'                        messages exceeds this value.\n' +
'    --flow-resume-count=<n>\n' +
'                        Turn off sender flow control when the number of queued\n' +
'                        messages drops below this value.\n' +
'    --group-header=<header-name>\n' +
'                        Enable message groups. Specify name of header that\n' +
'                        holds group identifier.\n' +
'    --shared-groups     Allow message group consumption across multiple\n' +
'                        consumers.\n' +
'    --argument=<NAME=VALUE>\n' +
'                        Specify a key-value pair to add to queue arguments\n' +
'    --start-replica=<broker-url>\n' +
'                        Start replication from the same-named queue at\n' +
'                        <broker-url>\n' +
'\n' +
'  Options for Adding Exchanges:\n' +
'    --sequence          Exchange will insert a \'qpid.msg_sequence\' field in\n' +
'                        the message header\n' +
'    --ive               Exchange will behave as an \'initial-value-exchange\',\n' +
'                        keeping a reference  to the last message forwarded and\n' +
'                        enqueuing that message to newly bound queues.\n' +
'\n' +
'  Options for Deleting Queues:\n' +
'    --force             Force delete of queue even if it\'s currently used or\n' +
'                        it\'s not empty\n' +
'    --force-if-not-empty\n' +
'                        Force delete of queue even if it\'s not empty\n' +
'    --force-if-used     Force delete of queue even if it\'s currently used\n' +
'\n' +
'  Options for Declaring Bindings:\n' +
'    -f <file.xq>, --file=<file.xq>\n' +
'                        For XML Exchange bindings - specifies the name of a\n' +
'                        file containing an XQuery.\n' +
'\n' +
'  Formatting options for \'list\' action:\n' +
'    --show-property=<property-name>\n' +
'                        Specify a property of an object to be included in\n' +
'                        output\n';

var REPLICATE_LEVELS = {"none" : true, "configuration": true, "all": true};
var DEFAULT_PROPERTIES = {"exchange": {"name": true, "type": true, "durable": true},
                             "queue": {"name": true, "durable": true, "autoDelete": true}};

var getValue = function(r) {
    var value = null;
    if (r.length === 2) {
        value = r[1];
        if (!isNaN(value)) {
            value = parseInt(value);
        }
    }

    return value;
};

var config = {
    _recursive      : false,
    _host           : 'localhost:5673', // Note 5673 not 5672 as we use WebSocket transport.
    _connTimeout    : 10,
    _ignoreDefault  : false,
    _altern_ex      : null,
    _durable        : false,
    _replicate      : null,
    _if_empty       : true,
    _if_unused      : true,
    _fileCount      : null,
    _fileSize       : null,
    _maxQueueSize   : null,
    _maxQueueCount  : null,
    _limitPolicy    : null,
    _msgSequence    : false,
    _lvq_key        : null,
    _ive            : null,
    _eventGeneration: null,
    _file           : null,
    _flowStopCount  : null,
    _flowResumeCount: null,
    _flowStopSize   : null,
    _flowResumeSize : null,
    _msgGroupHeader : null,
    _sharedMsgGroup : false,
    _extra_arguments: [],
    _start_replica  : null,
    _returnCode     : 0,
    _list_properties: null,

    getOptions: function() {
        var options = {};
        for (var a = 0; a < this._extra_arguments.length; a++) {
            var r = this._extra_arguments[a].split('=');
            options[r[0]] = getValue(r);
        }
        return options;
    }
};

var FILECOUNT = 'qpid.file_count';
var FILESIZE  = 'qpid.file_size';
var MAX_QUEUE_SIZE  = 'qpid.max_size';
var MAX_QUEUE_COUNT  = 'qpid.max_count';
var POLICY_TYPE  = 'qpid.policy_type';
var LVQ_KEY = 'qpid.last_value_queue_key';
var MSG_SEQUENCE = 'qpid.msg_sequence';
var IVE = 'qpid.ive';
var QUEUE_EVENT_GENERATION = 'qpid.queue_event_generation';
var FLOW_STOP_COUNT   = 'qpid.flow_stop_count';
var FLOW_RESUME_COUNT = 'qpid.flow_resume_count';
var FLOW_STOP_SIZE    = 'qpid.flow_stop_size';
var FLOW_RESUME_SIZE  = 'qpid.flow_resume_size';
var MSG_GROUP_HDR_KEY = 'qpid.group_header_key';
var SHARED_MSG_GROUP  = 'qpid.shared_msg_group';
var REPLICATE = 'qpid.replicate';

/**
 * There are various arguments to declare that have specific program
 * options in this utility. However there is now a generic mechanism for
 * passing arguments as well. The SPECIAL_ARGS list contains the
 * arguments for which there are specific program options defined
 * i.e. the arguments for which there is special processing on add and
 * list
*/
var SPECIAL_ARGS={};
SPECIAL_ARGS[FILECOUNT] = true;
SPECIAL_ARGS[FILESIZE] = true;
SPECIAL_ARGS[MAX_QUEUE_SIZE] = true;
SPECIAL_ARGS[MAX_QUEUE_COUNT] = true;
SPECIAL_ARGS[POLICY_TYPE] = true;
SPECIAL_ARGS[LVQ_KEY] = true;
SPECIAL_ARGS[MSG_SEQUENCE] = true;
SPECIAL_ARGS[IVE] = true;
SPECIAL_ARGS[QUEUE_EVENT_GENERATION] = true;
SPECIAL_ARGS[FLOW_STOP_COUNT] = true;
SPECIAL_ARGS[FLOW_RESUME_COUNT] = true;
SPECIAL_ARGS[FLOW_STOP_SIZE] = true;
SPECIAL_ARGS[FLOW_RESUME_SIZE] = true;
SPECIAL_ARGS[MSG_GROUP_HDR_KEY] = true;
SPECIAL_ARGS[SHARED_MSG_GROUP] = true;
SPECIAL_ARGS[REPLICATE] = true;

var oid = function(id) {
    return id._agent_epoch + ':' + id._object_name
};

var filterMatch = function(name, filter) {
    if (filter === '') {
        return true;
    }
    if (name.indexOf(filter) === -1) {
        return false;
    }
    return true;
};

var idMap = function(list) {
    var map = {};
    for (var i = 0; i < list.length; i++) {
        var item = list[i];
        map[oid(item._object_id)] = item;
    }
    return map;
};

var renderObject = function(obj, list) {
    if (!obj) {
        return '';
    }
    var string = '';
    var addComma = false;
    for (var prop in obj) {
        if (addComma) {
            string += ', ';
        }
        if (obj.hasOwnProperty(prop)) {
            if (list) {
                if (SPECIAL_ARGS[prop]) continue;
                string += " --argument " + prop + "=" + obj[prop];
            } else {    
                string += "'" + prop + "'" + ": '" + obj[prop] + "'";
                addComma = true;
            }
        }
    }

    if (addComma) {
        return '{' + string + '}';
    } else {
        if (list) {
            return string;
        } else {
            return '';
        }
    }
};

/**
 * The following methods illustrate the QMF2 class query mechanism which returns
 * the list of QMF Objects for the specified class that are currently present
 * on the Broker. The Schema <qpid>/cpp/src/qpid/broker/management-schema.xml
 * describes the properties and statistics of each Management Object.
 * <p>
 * One slightly subtle part of QMF is that certain Objects are associated via
 * references, for example Binding contains queueRef and exchangeRef, which lets
 * Objects link to each other using their _object_id property.
 * <p>
 * The implementation of these methods attempts to follow the same general flow
 * as the equivalent method in the "canonical" python based qpid-config version
 * but has the added complication that JavaScript is entirely asynchronous.
 * The approach that has been taken is to use the correlator object that lets a
 * callback function be registered via the "then" method and actually calls the
 * callback when all of the requests specified in the request method have
 * returned their results (which get passed as the callback parameter).
 */

var overview = function() {
    correlator.request(
        // Send the QMF query requests for the specified classes.
        getObjects('org.apache.qpid.broker', 'queue'),
        getObjects('org.apache.qpid.broker', 'exchange')
    ).then(function(objects) {
        var exchanges = objects.exchange;
        var queues = objects.queue;
        console.log("Total Exchanges: " + exchanges.length);
        var etype = {};
        for (var i = 0; i < exchanges.length; i++) {
            var exchange = exchanges[i]._values;
            if (!etype[exchange.type]) {
                etype[exchange.type] = 1;
            } else {
                etype[exchange.type]++;
            }
        }
        for (var typ in etype) {
            var pad = Array(16 - typ.length).join(' ');
            console.log(pad + typ + ": " + etype[typ]);
        }

        console.log("\n   Total Queues: " + queues.length);
        var durable = 0;
        for (var i = 0; i < queues.length; i++) {
            var queue = queues[i]._values;
            if (queue.durable) {
                durable++;
            }
        }
        console.log("        durable: " + durable);
        console.log("    non-durable: " + (queues.length - durable));
        messenger.stop();
    });
};

var exchangeList = function(filter) {
    correlator.request(
        // Send the QMF query requests for the specified classes.
        getObjects('org.apache.qpid.broker', 'exchange')
    ).then(function(objects) {
        var exchanges = objects.exchange;
        var exMap = idMap(exchanges);
        var caption1 = "Type      ";
        var caption2 = "Exchange Name";
        var maxNameLen = caption2.length;
        var found = false;
        for (var i = 0; i < exchanges.length; i++) {
            var exchange = exchanges[i]._values;
            if (filterMatch(exchange.name, filter)) {
                if (exchange.name.length > maxNameLen) {
                    maxNameLen = exchange.name.length;
                }
                found = true;
            }
        }
        if (!found) {
            config._returnCode = 1;
            return;
        }

        var pad = Array(maxNameLen + 1 - caption2.length).join(' ');
        console.log(caption1 + caption2 + pad + "  Attributes");
        console.log(Array(maxNameLen + caption1.length + 13).join('='));

        for (var i = 0; i < exchanges.length; i++) {
            var exchange = exchanges[i]._values;
            if (config._ignoreDefault && !exchange.name) continue;
            if (filterMatch(exchange.name, filter)) {
                var pad1 = Array(11 - exchange.type.length).join(' ');
                var pad2 = Array(maxNameLen + 2 - exchange.name.length).join(' ');
                var string = exchange.type + pad1 + exchange.name + pad2;
                var args = exchange.arguments ? exchange.arguments : {};
                if (exchange.durable) {
                    string += ' --durable';
                }
                if (args[REPLICATE]) {
                    string += ' --replicate=' + args[REPLICATE];
                }
                if (args[MSG_SEQUENCE]) {
                    string += ' --sequence';
                }
                if (args[IVE]) {
                    string += ' --ive';
                }
                if (exchange.altExchange) {
                    string += ' --alternate-exchange=' + exMap[oid(exchange.altExchange)]._values.name;
                }
                console.log(string);
            }
        }
        messenger.stop();
    });
};

var exchangeListRecurse = function(filter) {
    correlator.request(
        // Send the QMF query requests for the specified classes.
        getObjects('org.apache.qpid.broker', 'queue'),
        getObjects('org.apache.qpid.broker', 'exchange'),
        getObjects('org.apache.qpid.broker', 'binding')
    ).then(function(objects) {
        var exchanges = objects.exchange;
        var bindings = objects.binding;
        var queues = idMap(objects.queue);

        for (var i = 0; i < exchanges.length; i++) {
            var exchange = exchanges[i];
            var exchangeId = oid(exchange._object_id);
            exchange = exchange._values;

            if (config._ignoreDefault && !exchange.name) continue;
            if (filterMatch(exchange.name, filter)) {
                console.log("Exchange '" + exchange.name + "' (" + exchange.type + ")");
                for (var j = 0; j < bindings.length; j++) {
                    var bind = bindings[j]._values;
                    var exchangeRef = oid(bind.exchangeRef);

                    if (exchangeRef === exchangeId) {
                        var queue = queues[oid(bind.queueRef)];
                        var queueName = queue ? queue._values.name : "<unknown>";
                        console.log("    bind [" + bind.bindingKey + "] => " + queueName + 
                                    " " + renderObject(bind.arguments));
                    }   
                }
            }
        }
        messenger.stop();
    });
};

var queueList = function(filter) {
    correlator.request(
        // Send the QMF query requests for the specified classes.
        getObjects('org.apache.qpid.broker', 'queue'),
        getObjects('org.apache.qpid.broker', 'exchange')
    ).then(function(objects) {
        var queues = objects.queue;
        var exMap = idMap(objects.exchange);
        var caption = "Queue Name";
        var maxNameLen = caption.length;
        var found = false;
        for (var i = 0; i < queues.length; i++) {
            var queue = queues[i]._values;
            if (filterMatch(queue.name, filter)) {
                if (queue.name.length > maxNameLen) {
                    maxNameLen = queue.name.length;
                }
                found = true;
            }
        }
        if (!found) {
            config._returnCode = 1;
            return;
        }

        var pad = Array(maxNameLen + 1 - caption.length).join(' ');
        console.log(caption + pad + "  Attributes");
        console.log(Array(maxNameLen + caption.length + 3).join('='));

        for (var i = 0; i < queues.length; i++) {
            var queue = queues[i]._values;
            if (filterMatch(queue.name, filter)) {
                var pad2 = Array(maxNameLen + 2 - queue.name.length).join(' ');
                var string = queue.name + pad2;
                var args = queue.arguments ? queue.arguments : {};
                if (queue.durable) {
                    string += ' --durable';
                }
                if (args[REPLICATE]) {
                    string += ' --replicate=' + args[REPLICATE];
                }
                if (queue.autoDelete) {
                    string += ' auto-del';
                }
                if (queue.exclusive) {
                    string += ' excl';
                }
                if (args[FILESIZE]) {
                    string += ' --file-size=' + args[FILESIZE];
                }
                if (args[FILECOUNT]) {
                    string += ' --file-count=' + args[FILECOUNT];
                }
                if (args[MAX_QUEUE_SIZE]) {
                    string += ' --max-queue-size=' + args[MAX_QUEUE_SIZE];
                }
                if (args[MAX_QUEUE_COUNT]) {
                    string += ' --max-queue-count=' + args[MAX_QUEUE_COUNT];
                }
                if (args[POLICY_TYPE]) {
                    string += ' --limit-policy=' + args[POLICY_TYPE].replace("_", "-");
                }
                if (args[LVQ_KEY]) {
                    string += ' --lvq-key=' + args[LVQ_KEY];
                }
                if (args[QUEUE_EVENT_GENERATION]) {
                    string += ' --generate-queue-events=' + args[QUEUE_EVENT_GENERATION];
                }
                if (queue.altExchange) {
                    string += ' --alternate-exchange=' + exMap[oid(queue.altExchange)]._values.name;
                }
                if (args[FLOW_STOP_SIZE]) {
                    string += ' --flow-stop-size=' + args[FLOW_STOP_SIZE];
                }
                if (args[FLOW_RESUME_SIZE]) {
                    string += ' --flow-resume-size=' + args[FLOW_RESUME_SIZE];
                }
                if (args[FLOW_STOP_COUNT]) {
                    string += ' --flow-stop-count=' + args[FLOW_STOP_COUNT];
                }
                if (args[FLOW_RESUME_COUNT]) {
                    string += ' --flow-resume-count=' + args[FLOW_RESUME_COUNT];
                }
                if (args[MSG_GROUP_HDR_KEY]) {
                    string += ' --group-header=' + args[MSG_GROUP_HDR_KEY];
                }
                if (args[SHARED_MSG_GROUP] === 1) {
                    string += ' --shared-groups';
                }
                string += ' ' + renderObject(args, true);
                console.log(string);
            }
        }
        messenger.stop();
    });
};

var queueListRecurse = function(filter) {
    correlator.request(
        // Send the QMF query requests for the specified classes.
        getObjects('org.apache.qpid.broker', 'queue'),
        getObjects('org.apache.qpid.broker', 'exchange'),
        getObjects('org.apache.qpid.broker', 'binding')
    ).then(function(objects) {
        var queues = objects.queue;
        var bindings = objects.binding;
        var exchanges = idMap(objects.exchange);

        for (var i = 0; i < queues.length; i++) {
            var queue = queues[i];
            var queueId = oid(queue._object_id);
            queue = queue._values;

            if (filterMatch(queue.name, filter)) {
                console.log("Queue '" + queue.name + "'");
                for (var j = 0; j < bindings.length; j++) {
                    var bind = bindings[j]._values;
                    var queueRef = oid(bind.queueRef);

                    if (queueRef === queueId) {
                        var exchange = exchanges[oid(bind.exchangeRef)];
                        var exchangeName = "<unknown>";
                        if (exchange) {
                            exchangeName = exchange._values.name;
                            if (exchangeName === '') {
                                if (config._ignoreDefault) continue;
                                exchangeName = "''";
                            }
                        }

                        console.log("    bind [" + bind.bindingKey + "] => " + exchangeName + 
                                    " " + renderObject(bind.arguments));
                    }   
                }
            }
        }
        messenger.stop();
    });
};

/**
 * The following methods implement adding and deleting various Broker Management
 * Objects via QMF. Although <qpid>/cpp/src/qpid/broker/management-schema.xml
 * describes the basic method schema, for example:
 *   <method name="create" desc="Create an object of the specified type">
 *     <arg name="type" dir="I" type="sstr" desc="The type of object to create"/>
 *     <arg name="name" dir="I" type="sstr" desc="The name of the object to create"/>
 *     <arg name="properties" dir="I" type="map" desc="Type specific object properties"/>
 *     <arg name="strict" dir="I" type="bool" desc="If specified, treat unrecognised object properties as an error"/>
 *   </method>
 *
 *   <method name="delete" desc="Delete an object of the specified type">
 *     <arg name="type" dir="I" type="sstr" desc="The type of object to delete"/>
 *     <arg name="name" dir="I" type="sstr" desc="The name of the object to delete"/>
 *     <arg name="options" dir="I" type="map" desc="Type specific object options for deletion"/>
 *   </method>
 *
 * What the schema doesn't do however is to explain what the properties/options
 * Map values actually mean, unfortunately these aren't documented anywhere so
 * the only option is to look in the code, the best place to look is in:
 * <qpid>/cpp/src/qpid/broker/Broker.cpp, the method Broker::ManagementMethod is
 * the best place to start, then Broker::createObject and Broker::deleteObject
 * even then it's pretty hard to figure out all that is possible.
 */

var handleMethodResponse = function(response, dontStop) {
console.log("Method result");
    if (response._arguments) {
        //console.log(response._arguments);
    } if (response._values) {
        console.error("Exception from Agent: " + renderObject(response._values));
    }
    // Mostly we want to stop the Messenger Event loop and exit when a QMF method
    // returns, but sometimes we don't, the dontStop flag prevents this behaviour.
    if (!dontStop) {
        messenger.stop();
    }
}

var addExchange = function(args) {
    if (args.length < 2) {
        usage();
    }

    var etype = args[0];
    var ename = args[1];
    var declArgs = {};

    declArgs['exchange-type'] = etype;

    for (var a = 0; a < config._extra_arguments.length; a++) {
        var r = config._extra_arguments[a].split('=');
        declArgs[r[0]] = getValue(r);
    }

    if (config._msgSequence) {
        declArgs[MSG_SEQUENCE] = 1;
    }

    if (config._ive) {
        declArgs[IVE] = 1;
    }

    if (config._altern_ex) {
        declArgs['alternate-exchange'] = config._altern_ex;
    }

    if (config._durable) {
        declArgs['durable'] = 1;
    }

    if (config._replicate) {
        declArgs[REPLICATE] = config._replicate;
    }

    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'create', {
                "type":      "exchange",
                "name":       ename,
                "properties": declArgs,
                "strict":     true})
        ).then(handleMethodResponse);
    });
};

var delExchange = function(args) {
    if (args.length < 1) {
        usage();
    }

    var ename = args[0];

    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'delete', {
                "type":   "exchange",
                "name":    ename})
        ).then(handleMethodResponse);
    });
};

var addQueue = function(args) {
    if (args.length < 1) {
        usage();
    }

    var qname = args[0];
    var declArgs = {};

    for (var a = 0; a < config._extra_arguments.length; a++) {
        var r = config._extra_arguments[a].split('=');
        declArgs[r[0]] = getValue(r);
    }

    if (config._durable) {
        // allow the default fileCount and fileSize specified 
        // in qpid config file to take prededence
        if (config._fileCount) {
            declArgs[FILECOUNT] = config._fileCount;
        }
        if (config._fileSize) {
            declArgs[FILESIZE]  = config._fileSize;
        }
    }

    if (config._maxQueueSize != null) {
        declArgs[MAX_QUEUE_SIZE] = config._maxQueueSize;
    }

    if (config._maxQueueCount != null) {
        declArgs[MAX_QUEUE_COUNT] = config._maxQueueCount;
    }
    
    if (config._limitPolicy) {
        if (config._limitPolicy === 'none') {
        } else if (config._limitPolicy === 'reject') {
            declArgs[POLICY_TYPE] = 'reject';
        } else if (config._limitPolicy === 'ring') {
            declArgs[POLICY_TYPE] = 'ring';
        }
    }

    if (config._lvq_key) {
        declArgs[LVQ_KEY] = config._lvq_key;
    }

    if (config._eventGeneration) {
        declArgs[QUEUE_EVENT_GENERATION] = config._eventGeneration;
    }

    if (config._flowStopSize != null) {
        declArgs[FLOW_STOP_SIZE] = config._flowStopSize;
    }

    if (config._flowResumeSize != null) {
        declArgs[FLOW_RESUME_SIZE] = config._flowResumeSize;
    }

    if (config._flowStopCount != null) {
        declArgs[FLOW_STOP_COUNT] = config._flowStopCount;
    }

    if (config._flowResumeCount != null) {
        declArgs[FLOW_RESUME_COUNT] = config._flowResumeCount;
    }

    if (config._msgGroupHeader) {
        declArgs[MSG_GROUP_HDR_KEY] = config._msgGroupHeader;
    }

    if (config._sharedMsgGroup) {
        declArgs[SHARED_MSG_GROUP] = 1;
    }

    if (config._altern_ex) {
        declArgs['alternate-exchange'] = config._altern_ex;
    }

    if (config._durable) {
        declArgs['durable'] = 1;
    }

    if (config._replicate) {
        declArgs[REPLICATE] = config._replicate;
    }

    // This block is a little complex and untidy, the real issue is that the
    // correlator object isn't as good as a real Promise and doesn't support
    // chaining of "then" calls, so where we have complex dependencies we still
    // get somewhat into "callback hell". TODO improve the correlator.
    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'create', {
                "type":      "queue",
                "name":       qname,
                "properties": declArgs,
                "strict":     true})
        ).then(function(response) {
            if (config._start_replica) {
                handleMethodResponse(response, true); // The second parameter prevents exiting.
                // TODO test this stuff!
                correlator.request(
                    getObjects('org.apache.qpid.ha', 'habroker') // Not sure if this is correct
                ).then(function(objects) {
                    if (objects.habroker.length > 0) {
                        var habroker = objects.habroker[0];
                        correlator.request(
                            invokeMethod(habroker, 'replicate', {
                                "broker": config._start_replica,
                                "queue":  qname})
                        ).then(handleMethodResponse);
                    } else {
                        messenger.stop();
                    }
                });
            } else {
                handleMethodResponse(response);
            }
        });
    });
};

var delQueue = function(args) {
    if (args.length < 1) {
        usage();
    }

    var qname = args[0];

    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'delete', {
                "type":   "queue",
                "name":    qname,
                "options": {"if_empty":  config._if_empty,
                            "if_unused": config._if_unused}})
        ).then(handleMethodResponse);
    });
};

var snarf_header_args = function(args) {
    if (args.length < 2) {
        console.log("Invalid args to bind headers: need 'any'/'all' plus conditions");
        return false;
    }

    var op = args[0];
    if (op === 'all' || op === 'any') {
        kv = {};
        var bindings = Array.prototype.slice.apply(args, [1]);
        for (var i = 0; i < bindings.length; i++) {
            var binding = bindings[i];
            binding = binding.split(",")[0];
            binding = binding.split("=");
            kv[binding[0]] = binding[1];
        }
        kv['x-match'] = op;
        return kv;
    } else {
        console.log("Invalid condition arg to bind headers, need 'any' or 'all', not '" + op + "'");
        return false;
    }
};

var bind = function(args) {
console.log("bind");
console.log(args);

    if (args.length < 2) {
        usage();
    }

    var ename = args[0];
    var qname = args[1];
    var key   = '';

    if (args.length > 2) {
        key = args[2];
    }

    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker'),
        getObjects('org.apache.qpid.broker', 'exchange') // Get exchanges to look up exchange type.
    ).then(function(objects) {
        var exchanges = objects.exchange;

        var etype = '';
        for (var i = 0; i < exchanges.length; i++) {
            var exchange = exchanges[i]._values;
            if (exchange.name === ename) {
                etype = exchange.type;
                break;
            }
        }

        // type of the xchg determines the processing of the rest of
        // argv.  if it's an xml xchg, we want to find a file
        // containing an x-query, and pass that.  if it's a headers
        // exchange, we need to pass either "any" or all, followed by a
        // map containing key/value pairs.  if neither of those, extra
        // args are ignored.
        var declArgs = {};
        if (etype === 'xml') {


        } else if (etype === 'headers') {
            declArgs = snarf_header_args(Array.prototype.slice.apply(args, [3]));
        }
console.log(declArgs);

        if (typeof declArgs !== 'object') {
            process.exit(1);
        }

        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'create', {
                "type":   "binding",
                "name":    ename + '/' + qname + '/' + key,
                "properties": declArgs,
                "strict":     true})
        ).then(handleMethodResponse);
    });

/*

        ok = True
        _args = {}
        if not res:
            pass
        elif res.type == "xml":
            # this checks/imports the -f arg
            [ok, xquery] = snarf_xquery_args()
            _args = { "xquery" : xquery }
        else:
            if res.type == "headers":
                [ok, op, kv] = snarf_header_args(args[3:])
                _args = kv
                _args["x-match"] = op

        if not ok:
            sys.exit(1)

        self.broker.bind(ename, qname, key, _args)
*/

};

var unbind = function(args) {
console.log("unbind");
console.log(args);

    if (args.length < 2) {
        usage();
    }

    var ename = args[0];
    var qname = args[1];
    var key   = '';

    if (args.length > 2) {
        key = args[2];
    }

    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            invokeMethod(broker, 'delete', {
                "type":   "binding",
                "name":    ename + '/' + qname + '/' + key})
        ).then(handleMethodResponse);
    });
};

/**
 * The following methods are "generic" create and delete methods to for arbitrary
 * Management Objects e.g. Incoming, Outgoing, Domain, Topic, QueuePolicy,
 * TopicPolicy etc. use --argument k1=v1 --argument k2=v2 --argument k3=v3 to
 * pass arbitrary arguments as key/value pairs to the Object being created/deleted,
 * for example to add a topic object that uses the fanout exchange:
 * ./qpid-config.js add topic fanout --argument exchange=amq.fanout \
 * --argument qpid.max_size=1000000 --argument qpid.policy_type=ring
 */

var createObject = function(type, name, args) {
    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            // Create an object of the specified type.
            invokeMethod(broker, 'create', {
                "type":       type,
                "name":       name,
                "properties": args,
                "strict":     true})
        ).then(handleMethodResponse);
    });
};

var deleteObject = function(type, name, args) {
    correlator.request(
        // We invoke the CRUD methods on the broker object.
        getObjects('org.apache.qpid.broker', 'broker')
    ).then(function(objects) {
        var broker = objects.broker[0];
        correlator.request(
            // Create an object of the specified type and name.
            invokeMethod(broker, 'delete', {
                "type":    type,
                "name":    name,
                "options": args})
        ).then(handleMethodResponse);
    });
};

/**
 * This is a "generic" mechanism for listing arbitrary Management Objects.
 */
var listObjects = function(type) {
    correlator.request(
        getObjects('org.apache.qpid.broker', type)
    ).then(function(objects) {
        // The correlator passes an object containing responses for all of the
        // supplied requests so we index it by the supplied type to get our response.
        objects = objects[type];

        // Collect available attributes, stringify the values and compute the max
        // length of the value of each attribute so that we can later create a table.
        var attributes = {};
        var lengths = {};
        for (var i = 0; i < objects.length; i++) {
            var object = objects[i];
            object = object._values;
            for (var prop in object) {
                if (typeof object[prop] === 'object') { // Stringify Object properties.
                    // Check if property is an ObjectID (reference property),
                    // if so replace with the "name" part of the OID.
                    if (object[prop]['_object_name']) {
                        var parts = object[prop]['_object_name'].split(':');
                        object[prop] = parts[parts.length - 1];
                    } else {
                        // Stringify general Object properties.
                        object[prop] = renderObject(object[prop]);
                    }
                } else {
                    object[prop] = object[prop].toString(); // Stringify other property types.
                }

                if (!lengths[prop] || object[prop].length > lengths[prop]) { // Compute lengths.
                    lengths[prop] = object[prop].length > prop.length ? object[prop].length : prop.length;
                }

                if (!config._list_properties || config._list_properties[prop]) { // Do we want this property?
                    attributes[prop] = true;
                }
            }
        }

        if (!config._list_properties && DEFAULT_PROPERTIES[type]) {
            attributes = DEFAULT_PROPERTIES[type];
        }

        // Using the information we've previously prepared now render a table
        // showing the required property values.
        var desired = [];
        var header = ''; // Table header showing the property names.
        if (attributes['name']) {
            desired.push('name');
            delete attributes['name'];
            header += 'name' + Array(lengths['name'] + 2 - 4).join(' ');
        }

        for (var prop in attributes) {
            desired.push(prop);
            header += prop + Array(lengths[prop] + 2 - prop.length).join(' ');
        }

        console.log("Objects of type '" + type + "'");
        console.log(header);
        console.log(Array(header.length).join('='));
        for (var i = 0; i < objects.length; i++) {
            var object = objects[i];
            object = object._values;
            var string = '';
            for (var j = 0; j < desired.length; j++) {
                var key = desired[j];
                string += object[key] + Array(lengths[key] + 2 - object[key].length).join(' ');
            }

            console.log(string);
        }

        messenger.stop();
    });
};

var reloadAcl = function() {
    correlator.request(
        getObjects('org.apache.qpid.acl', 'acl')
    ).then(function(objects) {
        if (objects.acl.length > 0) {
            var acl = objects.acl[0];
            correlator.request(
                // Create an object of the specified type.
                invokeMethod(acl, 'reloadACLFile', {})
            ).then(handleMethodResponse);
        } else {
            console.log("Failed: No ACL Loaded in Broker");
            messenger.stop();
        }
    });
};


/*********************** process command line options ************************/

var params = [];
var extra_arguments = [];
var args = process.argv.slice(2);
if (args.length > 0) {
    if (args[0] === '-h' || args[0] === '--help') {
        console.log(_usage);
        console.log(_description);
        console.log(_options);
        process.exit(0);
    }

    for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        if (arg === '-r' || arg === '--recursive') {
            config._recursive = true;
        } else if (arg === '--ignore-default') {
            config._ignoreDefault = true;
        } else if (arg === '--durable') {
            config._durable = true;
        } else if (arg === '--shared-groups') {
            config._sharedMsgGroup = true;
        } else if (arg === '--sequence') {
            config._sequence = true;
        } else if (arg === '--ive') {
            config._ive = true;
        } else if (arg === '--force') {
            config._if_empty = false;
            config._if_unused = false;
        } else if (arg === '--force-if-not-empty') {
            config._if_empty = false;
        } else if (arg === '--force-if-used') {
            config._if_unused = false;
        } else if (arg === '--sequence') {
            config._msgSequence = true;
        } else if (arg.charAt(0) === '-') {
            i++;
            var val = args[i];
            if (arg === '-t' || arg === '--timeout') {
                config._connTimeout = parseInt(val);
                if (config._connTimeout === 0) {
                    config._connTimeout = null;
                }
            } else if (arg === '-b' || arg === '--broker' || arg === '-a' || arg === '--broker-addr') {
                if (val != null) {
                    config._host = val;
                }
            } else if (arg === '--alternate-exchange') {
                config._altern_ex = val;
            } else if (arg === '--replicate') {
                if (!REPLICATE_LEVELS[val]) {
                    console.error("Invalid replication level " + val + ", should be one of 'none', 'configuration' or 'all'");
                }
                config._replicate = val;
            } else if (arg === '--file-count') {
                config._fileCount = parseInt(val);
            } else if (arg === '--file-size') {
                config._fileSize = parseInt(val);
            } else if (arg === '--max-queue-size') {
                config._maxQueueSize = parseInt(val);
            } else if (arg === '--max-queue-count') {
                config._maxQueueCount = parseInt(val);
            } else if (arg === '--limit-policy') {
                config._limitPolicy = val;
            } else if (arg === '--lvq-key') {
                config._lvq_key = val;
            } else if (arg === '--generate-queue-events') {
                config._eventGeneration = parseInt(val);
            } else if (arg === '--flow-stop-size') {
                config._flowStopSize = parseInt(val);
            } else if (arg === '--flow-resume-size') {
                config._flowResumeSize = parseInt(val);
            } else if (arg === '--flow-stop-count') {
                config._flowStopCount = parseInt(val);
            } else if (arg === '--flow-resume-count') {
                config._flowResumeCount = parseInt(val);
            } else if (arg === '--group-header') {
                config._msgGroupHeader = val;
            } else if (arg === '--argument') {
                extra_arguments.push(val);
            } else if (arg === '--start-replica') {
                config._start_replica = val;
            } else if (arg === '--f' || arg === '--file') { // TODO Won't work in node.js
                config._file = val;
            } else if (arg === '--show-property') {
                if (config._list_properties === null) {
                    config._list_properties = {};
                }
                config._list_properties[val] = true;
            }
        } else {
            params.push(arg);
        }
    }
}

config._extra_arguments = extra_arguments;

// The command only *actually* gets called when the QMF connection has actually
// been established so we wrap up the function we want to get called in a lambda.
var command = function() {overview();};
if (params.length > 0) {
    var cmd = params[0];
    var modifier = '';
    if (params.length > 1) {
        modifier = params[1];
    }

    if (cmd === 'exchanges') {
        if (config._recursive) {
            command = function() {exchangeListRecurse(modifier);};
        } else {
            command = function() {exchangeList(modifier);};
        }
    } else if (cmd === 'queues') {
        if (config._recursive) {
            command = function() {queueListRecurse(modifier);};
        } else {
            command = function() {queueList(modifier);};
        }
    } else if (cmd === 'add') {
        if (modifier === 'exchange') {
            command = function() {addExchange(Array.prototype.slice.apply(params, [2]));};
        } else if (modifier === 'queue') {
            command = function() {addQueue(Array.prototype.slice.apply(params, [2]));};
        } else if (params.length > 2) {
            command = function() {createObject(modifier, params[2], config.getOptions());};
        } else {
            usage();
        }
    } else if (cmd === 'del') {
        if (modifier === 'exchange') {
            command = function() {delExchange(Array.prototype.slice.apply(params, [2]));};
        } else if (modifier === 'queue') {
            command = function() {delQueue(Array.prototype.slice.apply(params, [2]));};
        } else if (params.length > 2) {
            command = function() {deleteObject(modifier, params[2], {});};
        } else {
            usage();
        }
    } else if (cmd === 'bind') {
        command = function() {bind(Array.prototype.slice.apply(params, [1]));};
    } else if (cmd === 'unbind') {
        command = function() {unbind(Array.prototype.slice.apply(params, [1]));};
    } else if (cmd === 'reload-acl') {
        command = function() {reloadAcl();};
    } else if (cmd === 'list' && params.length > 1) {
        command = function() {listObjects(modifier);};
    } else {
        usage();
    }
}

//console.log(config._host);


var onSubscription = function() {
    command();
};

