/*
 *
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


mergeInto(LibraryManager.library, {
// Add to main emscripten library.js
/*
  $PROTOCOLS: null,
  __buildProtocols__deps: ['$PROTOCOLS'],
  __buildProtocols: function() {

    // Allocate and populate a protoent structure given a name, protocol number and array of aliases
    function allocprotoent(name, proto, aliases) {
      // write name into buffer
      var nameBuf = _malloc(name.length + 1);
      writeAsciiToMemory(name, nameBuf);

      // write aliases into buffer
      var j = 0;
      var length = aliases.length;
      var aliasListBuf = _malloc((length + 1) * 4); // Use length + 1 so we have space for the terminating NULL pointer.

      for (var i = 0; i < length; i++, j += 4) {
        var alias = aliases[i];
        var aliasBuf = _malloc(alias.length + 1);
        writeAsciiToMemory(alias, aliasBuf);
        {{{ makeSetValue('aliasListBuf', 'j', 'aliasBuf', 'i8*') }}};
      }
      {{{ makeSetValue('aliasListBuf', 'j', '0', 'i8*') }}}; // Terminating NULL pointer.

      // generate protoent
      var pe = _malloc({{{ C_STRUCTS.protoent.__size__ }}});
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_name, 'nameBuf', 'i8*') }}};
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_aliases, 'aliasListBuf', 'i8**') }}};
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_proto, 'proto', 'i32') }}};
      return pe;
    };

    PROTOCOLS = {};
    PROTOCOLS['tcp'] = PROTOCOLS['6'] = allocprotoent('tcp', 6, ['TCP']);
    PROTOCOLS['udp'] = PROTOCOLS['17'] = allocprotoent('udp', 17, ['UDP']);
  },

  getprotobyname__deps: ['__buildProtocols', '$PROTOCOLS'],
  getprotobyname: function(name) {
    // struct protoent *getprotobyname (const char *);
    name = Pointer_stringify(name);
    if (!PROTOCOLS) {
        ___buildProtocols();
    }

    var result = PROTOCOLS[name];
    return result;
  },
  getprotobynumber__deps: ['getprotobyname'],
  getprotobynumber: function(number) {
    _getprotobyname(number);
  },


*/




// Hacks below
// -----------------------------------------------------------------------------------------------------------------


  $SOCKFS__postset: '__ATINIT__.push({ func: function() { SOCKFS.root = FS.mount(SOCKFS, {}, null); } });',
  $SOCKFS__deps: ['$FS'],
  $SOCKFS: {
    mount: function(mount) {
      return FS.createNode(null, '/', {{{ cDefine('S_IFDIR') }}} | 0777, 0);
    },
    createSocket: function(family, type, protocol) {
      var streaming = type == {{{ cDefine('SOCK_STREAM') }}};
      if (protocol) {
        assert(streaming == (protocol == {{{ cDefine('IPPROTO_TCP') }}})); // if SOCK_STREAM, must be tcp
      }

      // create our internal socket structure
      var sock = {
        family: family,
        type: type,
        protocol: protocol,
        server: null,
        peers: {},
        pending: [],
        recv_queue: [],
#if SOCKET_WEBRTC
#else
        sock_ops: SOCKFS.websocket_sock_ops
#endif
      };

      // create the filesystem node to store the socket structure
      var name = SOCKFS.nextname();
      var node = FS.createNode(SOCKFS.root, name, {{{ cDefine('S_IFSOCK') }}}, 0);
      node.sock = sock;

      // and the wrapping stream that enables library functions such
      // as read and write to indirectly interact with the socket
      var stream = FS.createStream({
        path: name,
        node: node,
        flags: FS.modeStringToFlags('r+'),
        seekable: false,
        stream_ops: SOCKFS.stream_ops
      });

      // map the new stream to the socket structure (sockets have a 1:1
      // relationship with a stream)
      sock.stream = stream;

      return sock;
    },
    getSocket: function(fd) {
      var stream = FS.getStream(fd);
      if (!stream || !FS.isSocket(stream.node.mode)) {
        return null;
      }
      return stream.node.sock;
    },
    // node and stream ops are backend agnostic
    stream_ops: {
      poll: function(stream) {
        var sock = stream.node.sock;
        return sock.sock_ops.poll(sock);
      },
      ioctl: function(stream, request, varargs) {
console.log('stream_ops.ioctl');
        var sock = stream.node.sock;
        return sock.sock_ops.ioctl(sock, request, varargs);
      },
      read: function(stream, buffer, offset, length, position /* ignored */) {
        var sock = stream.node.sock;
        var msg = sock.sock_ops.recvmsg(sock, length);
        if (!msg) {
          // socket is closed
          return 0;
        }
#if USE_TYPED_ARRAYS == 2
        buffer.set(msg.buffer, offset);
#else
        for (var i = 0; i < size; i++) {
          buffer[offset + i] = msg.buffer[i];
        }
#endif
        return msg.buffer.length;
      },
      write: function(stream, buffer, offset, length, position /* ignored */) {
        var sock = stream.node.sock;
        return sock.sock_ops.sendmsg(sock, buffer, offset, length);
      },
      close: function(stream) {
console.log('stream_ops.close');
        var sock = stream.node.sock;
        sock.sock_ops.close(sock);
      }
    },
    nextname: function() {
      if (!SOCKFS.nextname.current) {
        SOCKFS.nextname.current = 0;
      }
      return 'socket[' + (SOCKFS.nextname.current++) + ']';
    },
    // backend-specific stream ops
    websocket_sock_ops: {
      //
      // peers are a small wrapper around a WebSocket to help in
      // emulating dgram sockets
      //
      // these functions aren't actually sock_ops members, but we're
      // abusing the namespace to organize them
      //
      createPeer: function(sock, addr, port) {
        var ws;

        if (typeof addr === 'object') {
          ws = addr;
          addr = null;
          port = null;
        }

        if (ws) {
          // for sockets that've already connected (e.g. we're the server)
          // we can inspect the _socket property for the address
          if (ws._socket) {
            addr = ws._socket.remoteAddress;
            port = ws._socket.remotePort;
          }
          // if we're just now initializing a connection to the remote,
          // inspect the url property
          else {
            var result = /ws[s]?:\/\/([^:]+):(\d+)/.exec(ws.url);
            if (!result) {
              throw new Error('WebSocket URL must be in the format ws(s)://address:port');
            }
            addr = result[1];
            port = parseInt(result[2], 10);
          }
        } else {
          // create the actual websocket object and connect
          try {
            var url = 'ws://' + addr + ':' + port;
#if SOCKET_DEBUG
            console.log('connect: ' + url);
#endif
            // the node ws library API is slightly different than the browser's
            var opts = ENVIRONMENT_IS_NODE ? {headers: {'websocket-protocol': ['binary']}} : ['binary'];
            // If node we use the ws library.
            var WebSocket = ENVIRONMENT_IS_NODE ? require('ws') : window['WebSocket'];
            ws = new WebSocket(url, opts);
            ws.binaryType = 'arraybuffer';
          } catch (e) {
console.log('e: ' + e);
            throw new FS.ErrnoError(ERRNO_CODES.EHOSTUNREACH);
          }
        }

#if SOCKET_DEBUG
        Module.print('websocket adding peer: ' + addr + ':' + port);
#endif

        var peer = {
          addr: addr,
          port: port,
          socket: ws,
          dgram_send_queue: []
        };

        SOCKFS.websocket_sock_ops.addPeer(sock, peer);
        SOCKFS.websocket_sock_ops.handlePeerEvents(sock, peer);

        // if this is a bound dgram socket, send the port number first to allow
        // us to override the ephemeral port reported to us by remotePort on the
        // remote end.
        if (sock.type === {{{ cDefine('SOCK_DGRAM') }}} && typeof sock.sport !== 'undefined') {
#if SOCKET_DEBUG
          Module.print('websocket queuing port message (port ' + sock.sport + ')');
#endif
          peer.dgram_send_queue.push(new Uint8Array([
              255, 255, 255, 255,
              'p'.charCodeAt(0), 'o'.charCodeAt(0), 'r'.charCodeAt(0), 't'.charCodeAt(0),
              ((sock.sport & 0xff00) >> 8) , (sock.sport & 0xff)
          ]));
        }

        return peer;
      },
      getPeer: function(sock, addr, port) {
        return sock.peers[addr + ':' + port];
      },
      addPeer: function(sock, peer) {
        sock.peers[peer.addr + ':' + peer.port] = peer;
      },
      removePeer: function(sock, peer) {
        delete sock.peers[peer.addr + ':' + peer.port];
      },
      handlePeerEvents: function(sock, peer) {
        var first = true;

        var handleOpen = function () {
#if SOCKET_DEBUG
          Module.print('websocket handle open');
#endif
          try {
            var queued = peer.dgram_send_queue.shift();
            while (queued) {
#if SOCKET_DEBUG
              Module.print('websocket sending queued data (' + queued.byteLength + ' bytes): ' + [Array.prototype.slice.call(new Uint8Array(queued))]);
#endif
              peer.socket.send(queued);
              queued = peer.dgram_send_queue.shift();
            }
          } catch (e) {
            // not much we can do here in the way of proper error handling as we've already
            // lied and said this data was sent. shut it down.
            peer.socket.close();
          }
        };

        function handleMessage(data) {
          assert(typeof data !== 'string' && data.byteLength !== undefined);  // must receive an ArrayBuffer
          data = new Uint8Array(data);  // make a typed array view on the array buffer

#if SOCKET_DEBUG
          //Module.print('websocket handle message (' + data.byteLength + ' bytes): ' + [Array.prototype.slice.call(data)]);
          //Module.print('websocket handle message (' + data.byteLength + ' bytes)');
#endif

          // if this is the port message, override the peer's port with it
          var wasfirst = first;
          first = false;
          if (wasfirst &&
              data.length === 10 &&
              data[0] === 255 && data[1] === 255 && data[2] === 255 && data[3] === 255 &&
              data[4] === 'p'.charCodeAt(0) && data[5] === 'o'.charCodeAt(0) && data[6] === 'r'.charCodeAt(0) && data[7] === 't'.charCodeAt(0)) {
            // update the peer's port and it's key in the peer map
            var newport = ((data[8] << 8) | data[9]);
            SOCKFS.websocket_sock_ops.removePeer(sock, peer);
            peer.port = newport;
            SOCKFS.websocket_sock_ops.addPeer(sock, peer);
            return;
          }

          sock.recv_queue.push({ addr: peer.addr, port: peer.port, data: data });
        };

        if (ENVIRONMENT_IS_NODE) {
          peer.socket.on('open', handleOpen);
          peer.socket.on('message', function(data, flags) {
            if (!flags.binary) {
              return;
            }
            handleMessage((new Uint8Array(data)).buffer);  // copy from node Buffer -> ArrayBuffer
          });
          peer.socket.on('error', function(error) {
console.log('error ' + error);
            // don't throw
          });
        } else {
          peer.socket.onopen = handleOpen;
          peer.socket.onmessage = function peer_socket_onmessage(event) {
            handleMessage(event.data);
          };
        }
      },

      //
      // actual sock ops
      //
      poll: function(sock) {
        if (sock.type === {{{ cDefine('SOCK_STREAM') }}} && sock.server) {
          // listen sockets should only say they're available for reading
          // if there are pending clients.
          return sock.pending.length ? ({{{ cDefine('POLLRDNORM') }}} | {{{ cDefine('POLLIN') }}}) : 0;
        }

        var mask = 0;
        var dest = sock.type === {{{ cDefine('SOCK_STREAM') }}} ?  // we only care about the socket state for connection-based sockets
          SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport) :
          null;

        if (sock.recv_queue.length ||
            !dest ||  // connection-less sockets are always ready to read
            (dest && dest.socket.readyState === dest.socket.CLOSING) ||
            (dest && dest.socket.readyState === dest.socket.CLOSED)) {  // let recv return 0 once closed
          mask |= ({{{ cDefine('POLLRDNORM') }}} | {{{ cDefine('POLLIN') }}});
        }

        if (!dest ||  // connection-less sockets are always ready to write
            (dest && dest.socket.readyState === dest.socket.OPEN)) {
          mask |= {{{ cDefine('POLLOUT') }}};
        }

        if ((dest && dest.socket.readyState === dest.socket.CLOSING) ||
            (dest && dest.socket.readyState === dest.socket.CLOSED)) {
          mask |= {{{ cDefine('POLLHUP') }}};
        }
        return mask;
      },
      ioctl: function(sock, request, arg) {
console.log('ioctl');
        switch (request) {
          case {{{ cDefine('FIONREAD') }}}:
            var bytes = 0;
            if (sock.recv_queue.length) {
              bytes = sock.recv_queue[0].data.length;
            }
            {{{ makeSetValue('arg', '0', 'bytes', 'i32') }}};
            return 0;
          default:
            return ERRNO_CODES.EINVAL;
        }
      },
      close: function(sock) {
console.log('close');
        // if we've spawned a listen server, close it
        if (sock.server) {
          try {
            sock.server.close();
          } catch (e) {
          }
          sock.server = null;
        }
        // close any peer connections
        var peers = Object.keys(sock.peers);
        for (var i = 0; i < peers.length; i++) {
          var peer = sock.peers[peers[i]];
          try {
            peer.socket.close();
          } catch (e) {
          }
          SOCKFS.websocket_sock_ops.removePeer(sock, peer);
        }
        return 0;
      },
      bind: function(sock, addr, port) {
        if (typeof sock.saddr !== 'undefined' || typeof sock.sport !== 'undefined') {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);  // already bound
        }
        sock.saddr = addr;
        sock.sport = port || _mkport();
        // in order to emulate dgram sockets, we need to launch a listen server when
        // binding on a connection-less socket
        // note: this is only required on the server side
        if (sock.type === {{{ cDefine('SOCK_DGRAM') }}}) {
          // close the existing server if it exists
          if (sock.server) {
            sock.server.close();
            sock.server = null;
          }
          // swallow error operation not supported error that occurs when binding in the
          // browser where this isn't supported
          try {
            sock.sock_ops.listen(sock, 0);
          } catch (e) {
            if (!(e instanceof FS.ErrnoError)) throw e;
            if (e.errno !== ERRNO_CODES.EOPNOTSUPP) throw e;
          }
        }
      },
      connect: function(sock, addr, port) {
        if (sock.server) {
          throw new FS.ErrnoError(ERRNO_CODS.EOPNOTSUPP);
        }

        // TODO autobind
        // if (!sock.addr && sock.type == {{{ cDefine('SOCK_DGRAM') }}}) {
        // }

        // early out if we're already connected / in the middle of connecting
        if (typeof sock.daddr !== 'undefined' && typeof sock.dport !== 'undefined') {
          var dest = SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport);
          if (dest) {
            if (dest.socket.readyState === dest.socket.CONNECTING) {
              throw new FS.ErrnoError(ERRNO_CODES.EALREADY);
            } else {
              throw new FS.ErrnoError(ERRNO_CODES.EISCONN);
            }
          }
        }

        // add the socket to our peer list and set our
        // destination address / port to match
        var peer = SOCKFS.websocket_sock_ops.createPeer(sock, addr, port);
        sock.daddr = peer.addr;
        sock.dport = peer.port;

        // always "fail" in non-blocking mode
        throw new FS.ErrnoError(ERRNO_CODES.EINPROGRESS);
      },
      listen: function(sock, backlog) {
        if (!ENVIRONMENT_IS_NODE) {
          throw new FS.ErrnoError(ERRNO_CODES.EOPNOTSUPP);
        }
        if (sock.server) {
           throw new FS.ErrnoError(ERRNO_CODES.EINVAL);  // already listening
        }
        var WebSocketServer = require('ws').Server;
        var host = sock.saddr;
#if SOCKET_DEBUG
        console.log('listen: ' + host + ':' + sock.sport);
#endif
        sock.server = new WebSocketServer({
          host: host,
          port: sock.sport
          // TODO support backlog
        });

        sock.server.on('connection', function(ws) {
#if SOCKET_DEBUG
          console.log('received connection from: ' + ws._socket.remoteAddress + ':' + ws._socket.remotePort);
#endif
          if (sock.type === {{{ cDefine('SOCK_STREAM') }}}) {
            var newsock = SOCKFS.createSocket(sock.family, sock.type, sock.protocol);

            // create a peer on the new socket
            var peer = SOCKFS.websocket_sock_ops.createPeer(newsock, ws);
            newsock.daddr = peer.addr;
            newsock.dport = peer.port;

            // push to queue for accept to pick up
            sock.pending.push(newsock);
          } else {
            // create a peer on the listen socket so calling sendto
            // with the listen socket and an address will resolve
            // to the correct client
            SOCKFS.websocket_sock_ops.createPeer(sock, ws);
          }
        });
        sock.server.on('closed', function() {
console.log('closed');
          sock.server = null;
        });
        sock.server.on('error', function() {
console.log('error');
          // don't throw
        });
      },
      accept: function(listensock) {
console.log('accept');
        if (!listensock.server) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }

        var newsock = listensock.pending.shift();
        newsock.stream.flags = listensock.stream.flags;
        return newsock;
      },
      getname: function(sock, peer) {
console.log('getname');
        var addr, port;
        if (peer) {
          if (sock.daddr === undefined || sock.dport === undefined) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
          }
          addr = sock.daddr;
          port = sock.dport;
        } else {
          // TODO saddr and sport will be set for bind()'d UDP sockets, but what
          // should we be returning for TCP sockets that've been connect()'d?
          addr = sock.saddr || 0;
          port = sock.sport || 0;
        }
        return { addr: addr, port: port };
      },
      sendmsg: function(sock, buffer, offset, length, addr, port) {
        if (sock.type === {{{ cDefine('SOCK_DGRAM') }}}) {
          // connection-less sockets will honor the message address,
          // and otherwise fall back to the bound destination address
          if (addr === undefined || port === undefined) {
            addr = sock.daddr;
            port = sock.dport;
          }
          // if there was no address to fall back to, error out
          if (addr === undefined || port === undefined) {
            throw new FS.ErrnoError(ERRNO_CODES.EDESTADDRREQ);
          }
        } else {
          // connection-based sockets will only use the bound
          addr = sock.daddr;
          port = sock.dport;
        }

        // find the peer for the destination address
        var dest = SOCKFS.websocket_sock_ops.getPeer(sock, addr, port);

        // early out if not connected with a connection-based socket
        if (sock.type === {{{ cDefine('SOCK_STREAM') }}}) {
          if (!dest || dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
            throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
          } else if (dest.socket.readyState === dest.socket.CONNECTING) {
            throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
          }
        }

        // create a copy of the incoming data to send, as the WebSocket API
        // doesn't work entirely with an ArrayBufferView, it'll just send
        // the entire underlying buffer
        var data;
        if (buffer instanceof Array || buffer instanceof ArrayBuffer) {
          data = buffer.slice(offset, offset + length);
        } else {  // ArrayBufferView
          data = buffer.buffer.slice(buffer.byteOffset + offset, buffer.byteOffset + offset + length);
        }

        // if we're emulating a connection-less dgram socket and don't have
        // a cached connection, queue the buffer to send upon connect and
        // lie, saying the data was sent now.
        if (sock.type === {{{ cDefine('SOCK_DGRAM') }}}) {
          if (!dest || dest.socket.readyState !== dest.socket.OPEN) {
            // if we're not connected, open a new connection
            if (!dest || dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
              dest = SOCKFS.websocket_sock_ops.createPeer(sock, addr, port);
            }
#if SOCKET_DEBUG
            Module.print('websocket queuing (' + length + ' bytes): ' + [Array.prototype.slice.call(new Uint8Array(data))]);
#endif
            dest.dgram_send_queue.push(data);
            return length;
          }
        }

        try {
#if SOCKET_DEBUG
          Module.print('websocket send (' + length + ' bytes): ' + [Array.prototype.slice.call(new Uint8Array(data))]);
#endif
          // send the actual data
          dest.socket.send(data);
          return length;
        } catch (e) {
          throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
        }
      },
      recvmsg: function(sock, length) {
        // http://pubs.opengroup.org/onlinepubs/7908799/xns/recvmsg.html
        if (sock.type === {{{ cDefine('SOCK_STREAM') }}} && sock.server) {
          // tcp servers should not be recv()'ing on the listen socket
          throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
        }

        var queued = sock.recv_queue.shift();
        if (!queued) {
          if (sock.type === {{{ cDefine('SOCK_STREAM') }}}) {
            var dest = SOCKFS.websocket_sock_ops.getPeer(sock, sock.daddr, sock.dport);

            if (!dest) {
              // if we have a destination address but are not connected, error out
              throw new FS.ErrnoError(ERRNO_CODES.ENOTCONN);
            }
            else if (dest.socket.readyState === dest.socket.CLOSING || dest.socket.readyState === dest.socket.CLOSED) {
              // return null if the socket has closed
              return null;
            }
            else {
              // else, our socket is in a valid state but truly has nothing available
              throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
            }
          } else {
            throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
          }
        }

        // queued.data will be an ArrayBuffer if it's unadulterated, but if it's
        // requeued TCP data it'll be an ArrayBufferView
        var queuedLength = queued.data.byteLength || queued.data.length;
        var queuedOffset = queued.data.byteOffset || 0;
        var queuedBuffer = queued.data.buffer || queued.data;
        var bytesRead = Math.min(length, queuedLength);
        var res = {
          buffer: new Uint8Array(queuedBuffer, queuedOffset, bytesRead),
          addr: queued.addr,
          port: queued.port
        };

#if SOCKET_DEBUG
        Module.print('websocket read (' + bytesRead + ' bytes): ' + [Array.prototype.slice.call(res.buffer)]);
#endif

        // push back any unread data for TCP connections
        if (sock.type === {{{ cDefine('SOCK_STREAM') }}} && bytesRead < queuedLength) {
          var bytesRemaining = queuedLength - bytesRead;
#if SOCKET_DEBUG
          Module.print('websocket read: put back ' + bytesRemaining + ' bytes');
#endif
          queued.data = new Uint8Array(queuedBuffer, queuedOffset + bytesRead, bytesRemaining);
          sock.recv_queue.unshift(queued);
        }

        return res;
      }
    }
  },



});
