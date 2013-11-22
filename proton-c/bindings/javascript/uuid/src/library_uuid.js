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

/**
 * This library is a simple emscripten "stub" library for libuuid. It uses the method described in
 * https://github.com/kripken/emscripten/wiki/Interacting-with-code Calling JavaScript From C/C++
 * and used by the built-in emscripten libraries and needs to be linked using: --js-library library_uuid.js
 * N.B. This is *not* a complete implementation of libuuid, rather it's primarily implementing uuid_unparse()
 * for use in proton's "pn_i_genuuid" function.
 */
mergeInto(LibraryManager.library, {
    uuid_generate: function(out) {
        // void uuid_generate(uuid_t out);
    },
    /**
     * Write a RFC4122 version 4 compliant UUID to the buffer pointed to by out using the method found in
     * http://stackoverflow.com/questions/105034/how-to-create-a-guid-uuid-in-javascript
     * Note that the real uuid_unparse takes a "compact" uuid created by uuid_generate, but for proton
     * we have a simple use-case of uuid_generate() immediately followed by uuid_unparse() and use no
     * other uuid functions, so we can cut a few corners.
     */
    uuid_unparse: function(uu, out) {
        // void uuid_unparse(const uuid_t uu, char *out);
        var d = new Date().getTime();
        var uuid = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            var r = (d + Math.random()*16)%16 | 0;
            d = Math.floor(d/16);
            return (c=='x' ? r : (r&0x7|0x8)).toString(16);
        });
        writeStringToMemory(uuid, out);
    }
});
