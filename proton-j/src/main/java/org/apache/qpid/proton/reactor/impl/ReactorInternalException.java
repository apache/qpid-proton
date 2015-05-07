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

package org.apache.qpid.proton.reactor.impl;

/**
 * Thrown by the reactor when it encounters an internal error condition.
 * This is analogous to an assertion failure in the proton-c reactor
 * implementation.
 */
class ReactorInternalException extends RuntimeException {

    private static final long serialVersionUID = 8979674526584642454L;

    protected ReactorInternalException(String msg) {
        super(msg);
    }

    protected ReactorInternalException(Throwable cause) {
        super(cause);
    }

    protected ReactorInternalException(String msg, Throwable cause) {
        super(msg, cause);
    }
}
