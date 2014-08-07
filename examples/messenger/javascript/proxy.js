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
 * proxy.js is a simple node.js command line application that uses the ws2tcp.js
 * library to proxy from a WebSocket to a TCP Socket or vice versa.
 * <p>
 * Usage: node proxy.js [options]
 * Options:");
 *  -p <listen port>, --port  <listen port> (default 5673 for ws2tcp
 *                                                   5672 for tcp2ws)
 *  -t <target port>, --tport <target port> (default listen port - 1 for ws2tcp
 *                                                   listen port + 1 for tcp2ws)
 *  -h <target host>, --thost <target host> (default 0.0.0.0)
 *  -m <ws2tcp or tcp2ws>, --method <ws2tcp or tcp2ws> (default ws2tcp)
 * @Author Fraser Adams
 * @file
 */

var proxy = require('./ws2tcp.js');

var lport = 5673;
var tport = lport - 1;
var thost = '0.0.0.0';
var method = 'ws2tcp';

var args = process.argv.slice(2);
if (args.length > 0) {
    if (args[0] === '-h' || args[0] === '--help') {
        console.log("Usage: node proxy.js [options]");
        console.log("Options:");
        console.log("  -p <listen port>, --port  <listen port> (default " + lport + " for ws2tcp");
        console.log("                                                   " + tport + " for tcp2ws)");
        console.log("  -t <target port>, --tport <target port> (default listen port - 1 for ws2tcp");
        console.log("                                                   listen port + 1 for tcp2ws)");
        console.log("  -h <target host>, --thost <target host> (default " + thost + ")");
        console.log("  -m <ws2tcp or tcp2ws>, --method <ws2tcp or tcp2ws> (default " + method + ")");
        process.exit(0);
    }

    var lportSet = false;
    var tportSet = false;
    for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        if (arg.charAt(0) === '-') {
            i++;
            var val = args[i];
            if (arg === '-p' || arg === '--port') {
                lport = val;
                lportSet = true;
            } else if (arg === '-t' || arg === '--tport') {
                tport = val;
                tportSet = true;
            } else if (arg === '-h' || arg === '--thost') {
                thost = val;
            } else if (arg === '-m' || arg === '--method') {
                method = val;
            }
        }
    }

    if (method === 'tcp2ws' && !lportSet) {
        lport--;
    }

    if (!tportSet) {
        tport = (method === 'ws2tcp') ? lport - 1 : +lport + 1;
    }
}

if (method === 'tcp2ws') {
    console.log("Proxying tcp -> ws");
    console.log("Forwarding port " + lport + " to " + thost + ":" + tport);
    proxy.tcp2ws(lport, thost, tport, 'AMQPWSB10');
} else if (method === 'ws2tcp') {
    console.log("Proxying ws -> tcp");
    console.log("Forwarding port " + lport + " to " + thost + ":" + tport);
    proxy.ws2tcp(lport, thost, tport);
} else {
    console.error("Method must be either ws2tcp or tcp2ws.");
}

