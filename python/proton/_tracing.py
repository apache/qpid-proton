#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import atexit
import functools
import os
import sys
import time
import weakref

try:
    import opentracing
    import jaeger_client
    from opentracing.ext import tags
    from opentracing.propagation import Format
except ImportError:
    raise ImportError('proton tracing requires opentracing and jaeger_client modules')

import proton
from proton import Sender as ProtonSender
from proton.handlers import (
    OutgoingMessageHandler as ProtonOutgoingMessageHandler,
    IncomingMessageHandler as ProtonIncomingMessageHandler
)

_tracer = None
_trace_key = proton.symbol('x-opt-qpid-tracestate')

def get_tracer():
    global _tracer
    if _tracer is not None:
        return _tracer
    exe = sys.argv[0] if sys.argv[0] else 'interactive-session'
    return init_tracer(os.path.basename(exe))

def _fini_tracer():
    time.sleep(1)
    c = opentracing.global_tracer().close()
    while not c.done():
        time.sleep(0.5)

def init_tracer(service_name):
    global _tracer
    if _tracer is not None:
        return _tracer

    config = jaeger_client.Config(
        config={},
        service_name=service_name,
        validate=True
    )
    config.initialize_tracer()
    _tracer = opentracing.global_tracer()
    # A nasty hack to ensure enough time for the tracing data to be flushed
    atexit.register(_fini_tracer)
    return _tracer


class IncomingMessageHandler(ProtonIncomingMessageHandler):
    def on_message(self, event):
        if self.delegate is not None:
            tracer = get_tracer()
            message = event.message
            receiver = event.receiver
            connection = event.connection
            span_tags = {
                tags.SPAN_KIND: tags.SPAN_KIND_CONSUMER,
                tags.MESSAGE_BUS_DESTINATION: receiver.source.address,
                tags.PEER_ADDRESS: connection.connected_address,
                tags.PEER_HOSTNAME: connection.hostname,
                'inserted_by': 'proton-message-tracing'
            }
            if message.annotations is not None:
                headers = message.annotations[_trace_key]
                span_ctx = tracer.extract(Format.TEXT_MAP, headers)
                with tracer.start_active_span('amqp-delivery-receive', child_of=span_ctx, tags=span_tags):
                    proton._events._dispatch(self.delegate, 'on_message', event)
            else:
                with tracer.start_active_span('amqp-delivery-receive', ignore_active_span=True, tags=span_tags):
                    proton._events._dispatch(self.delegate, 'on_message', event)

class OutgoingMessageHandler(ProtonOutgoingMessageHandler):
    def on_settled(self, event):
        if self.delegate is not None:
            delivery = event.delivery
            state = delivery.remote_state
            span = delivery.span
            span.set_tag('delivery-terminal-state', state.name)
            span.log_kv({'event': 'delivery settled', 'state': state.name})
            span.finish()
            proton._events._dispatch(self.delegate, 'on_settled', event)

class Sender(ProtonSender):
    def send(self, msg):
        tracer = get_tracer()
        connection = self.connection
        span_tags = {
            tags.SPAN_KIND: tags.SPAN_KIND_PRODUCER,
            tags.MESSAGE_BUS_DESTINATION: self.target.address,
            tags.PEER_ADDRESS: connection.connected_address,
            tags.PEER_HOSTNAME: connection.hostname,
            'inserted_by': 'proton-message-tracing'
        }
        span = tracer.start_span('amqp-delivery-send', tags=span_tags)
        headers = {}
        tracer.inject(span, Format.TEXT_MAP, headers)
        if msg.annotations is None:
            msg.annotations = { _trace_key: headers }
        else:
            msg.annotations[_trace_key] = headers
        delivery = ProtonSender.send(self, msg)
        delivery.span = span
        span.set_tag('delivery-tag', delivery.tag)
        return delivery

# Monkey patch proton for tracing (need to patch both internal and external names)
proton._handlers.IncomingMessageHandler = IncomingMessageHandler
proton._handlers.OutgoingMessageHandler = OutgoingMessageHandler
proton._endpoints.Sender = Sender
proton.handlers.IncomingMessageHandler = IncomingMessageHandler
proton.handlers.OutgoingMessageHandler = OutgoingMessageHandler
proton.Sender = Sender
