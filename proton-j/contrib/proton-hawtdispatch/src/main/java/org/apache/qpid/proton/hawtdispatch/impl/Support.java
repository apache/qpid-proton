/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.qpid.proton.hawtdispatch.impl;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class Support {

    public static IllegalStateException illegalState(String msg) {
        return (IllegalStateException) new IllegalStateException(msg).fillInStackTrace();
    }

    public static IllegalStateException createUnhandledEventError() {
        return illegalState("Unhandled event.");
    }

    public static IllegalStateException createListenerNotSetError() {
        return illegalState("No connection listener set to handle message received from the server.");
    }

    public static IllegalStateException createDisconnectedError() {
        return illegalState("Disconnected");
    }

}
