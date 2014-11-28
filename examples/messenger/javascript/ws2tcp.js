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
 * ws2tcp.js is a simple node.js library that proxies from a WebSocket to a TCP
 * Socket or vice versa. It has minimal dependencies - the standard node.js net
 * library and the ws WebSocket library (npm install ws).
 * <p>
 * Two fuctions are exported, ws2tcp proxies from a WebSocket to a TCP Socket and
 * tcp2ws proxies from a TCP Socket to a WebSocket.
 * @Author Fraser Adams
 * @file
 */

var WebSocket = require('ws');
var net = require('net');

/**
 * This function is shared by ws2tcp and tcp2ws and takes care of cleaning up
 * and closing the WebSocket and Socket when things close down or error.
 * @param sock the TCP Socket instance we're registering cleanup handlers for.
 * @param ws the WebSocket instance we're registering cleanup handlers for.
 */
var registerCleanupCallbacks = function(sock, ws) {
    var cleanup = function(sock, ws) {
        sock.removeAllListeners('close');	
        sock.end();
        ws.removeAllListeners('close');
        ws.close();
    };

    sock.on('close', function() {
        cleanup(sock, ws);
    });

    sock.on('error', function (e) {
        console.log("socket error: " + e.code);
        cleanup(sock, ws);
    });

    ws.on('close', function() {
        cleanup(sock, ws);
    });

    ws.on('error', function (e) {
        console.log("websocket error: " + e.code);
        cleanup(sock, ws);
    });
};

/**
 * This function establishes a proxy that listens on a specified TCP Socket port
 * and proxies data to a WebSocket on the target host listening on the specified
 * target port.
 * @param lport the listen port.
 * @param thost the target host.
 * @param tport the target port.
 * @param subProtocols a string containing a comma separated list of WebSocket sub-protocols.
 */
var tcp2ws = function(lport, thost, tport, subProtocols) {
    var opts = null;
    if (subProtocols) {
        // The regex trims the string (removes spaces at the beginning and end,
        // then splits the string by <any space>,<any space> into an Array.
        subProtocols = subProtocols.replace(/^ +| +$/g,"").split(/ *, */);
        opts = {'protocol': subProtocols.toString()};
    }

    var server = net.createServer(function(sock) {
        var url = 'ws://' + thost + ':' + tport;
        var ws = new WebSocket(url, opts);
        var ready = false;
        var buffer = [];

        registerCleanupCallbacks(sock, ws);

        sock.on('data', function(data) {
            if (ready) {
                ws.send(data);
            } else {
                buffer.push(data);
            }
        });

        ws.on('open', function () {
            if (buffer.length > 0) {
                ws.send(Buffer.concat(buffer));
            }
            ready = true;
            buffer = null;
        });

        ws.on('message', function(m) {
            sock.write(m);	
        });
    });
    server.listen(lport);
};

/**
 * This function establishes a proxy that listens on a specified WebSocket port
 * and proxies data to a TCP Socket on the target host listening on the specified
 * target port.
 * @param lport the listen port.
 * @param thost the target host.
 * @param tport the target port.
 */
var ws2tcp = function(lport, thost, tport) {
    var server = new WebSocket.Server({port: lport});
    server.on('connection', function(ws) {
        var sock = net.connect(tport, thost);
        var ready = false;
        var buffer = [];

        registerCleanupCallbacks(sock, ws);

        ws.on('message', function(m) {
            if (ready) {
                sock.write(m);	
            } else {
                buffer.push(m);
            }
        });

        sock.on('connect', function() {
            if (buffer.length > 0) {
                sock.write(Buffer.concat(buffer));
            }
            ready = true;
            buffer = null;
        });

        sock.on('data', function(data) {
            ws.send(data);
        });
    });
    server.on('error', function(e) {
        console.log("websocket server error: " + e.code);
    });
};

// Export the two proxy functions.
module.exports.ws2tcp = ws2tcp;
module.exports.tcp2ws = tcp2ws;

