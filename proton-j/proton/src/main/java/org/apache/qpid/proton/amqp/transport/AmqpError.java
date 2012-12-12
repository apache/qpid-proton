
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


package org.apache.qpid.proton.amqp.transport;

import org.apache.qpid.proton.amqp.Symbol;

public interface AmqpError
{
    final static Symbol INTERNAL_ERROR = Symbol.valueOf("amqp:internal-error");

    final static Symbol NOT_FOUND = Symbol.valueOf("amqp:not-found");

    final static Symbol UNAUTHORIZED_ACCESS = Symbol.valueOf("amqp:unauthorized-access");

    final static Symbol DECODE_ERROR = Symbol.valueOf("amqp:decode-error");

    final static Symbol RESOURCE_LIMIT_EXCEEDED = Symbol.valueOf("amqp:resource-limit-exceeded");

    final static Symbol NOT_ALLOWED = Symbol.valueOf("amqp:not-allowed");

    final static Symbol INVALID_FIELD = Symbol.valueOf("amqp:invalid-field");

    final static Symbol NOT_IMPLEMENTED = Symbol.valueOf("amqp:not-implemented");

    final static Symbol RESOURCE_LOCKED = Symbol.valueOf("amqp:resource-locked");

    final static Symbol PRECONDITION_FAILED = Symbol.valueOf("amqp:precondition-failed");

    final static Symbol RESOURCE_DELETED = Symbol.valueOf("amqp:resource-deleted");

    final static Symbol ILLEGAL_STATE = Symbol.valueOf("amqp:illegal-state");

    final static Symbol FRAME_SIZE_TOO_SMALL = Symbol.valueOf("amqp:frame-size-too-small");

}
