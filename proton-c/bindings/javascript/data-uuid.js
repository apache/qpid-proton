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

/*****************************************************************************/
/*                                                                           */
/*                             proton.Data.Uuid                              */
/*                                                                           */
/*****************************************************************************/

/**
 * Create a proton.Data.Uuid which is a type 4 UUID.
 * @classdesc
 * This class represents a type 4 UUID, wich may use crypto libraries to generate
 * the UUID if supported by the platform (e.g. node.js or a modern browser)
 * @constructor proton.Data.Uuid
 * @param {number|Array|string} u a UUID. If null a type 4 UUID is generated wich may use crypto if
 *        supported by the platform. If u is an emscripten "pointer" we copy the
 *        data from that. If u is a JavaScript Array we use it as-is. If u is a
 *        String then we try to parse that as a UUID.
 * @property {Array} uuid is the compact array form of the UUID.
 */
Data['Uuid'] = function(u) { // Data.Uuid Constructor.
    // Helper to copy from emscriptem allocated storage into JavaScript Array.
    function _p2a(p) {
        var uuid = new Array(16);
        for (var i = 0; i < 16; i++) {
            uuid[i] = getValue(p + i, 'i8') & 0xFF; // & 0xFF converts to unsigned.
        }
        return uuid;
    };

    if (!u) { // Generate UUID using emscriptem's uuid_generate implementation.
        var sp = Runtime.stackSave();
        var p = allocate(16, 'i8', ALLOC_STACK); // Create temporary pointer storage.
        _uuid_generate(p);      // Generate UUID into allocated pointer.
        this['uuid'] = _p2a(p); // Copy from allocated storage into JavaScript Array.
        Runtime.stackRestore(sp);
    } else if (Data.isNumber(u)) { // Use pointer that has been passed in.
        this['uuid'] = _p2a(u);    // Copy from allocated storage into JavaScript Array.
    } else if (Data.isArray(u)) { // Use array that has been passed in.
        this['uuid'] = u; // Just use the JavaScript Array.
    } else if (Data.isString(u)) { // Parse String form UUID.
        if (u.length === 36) {
            var i = 0;
            var uuid = new Array(16);
            u.toLowerCase().replace(/[0-9a-f]{2}/g, function(byte) {
                if (i < 16) {
                    uuid[i++] = parseInt(byte, 16);
                }
            });
            this['uuid'] = uuid;
        }
    }
    this.string = null;
};

/**
 * Returns the string representation of the proton.Data.Uuid.
 * @method toString
 * @memberof! proton.Data.Uuid#
 * @returns {string} the String
 *          /[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}/
 *          form of a {@link proton.Data.Uuid}.
 */
Data['Uuid'].prototype.toString = Data['Uuid'].prototype.valueOf = function() {
    if (!this.string) { // Check if we've cached the string version.
        var i = 0;
        var uu = this['uuid'];
        var uuid = 'xxxx-xx-xx-xx-xxxxxx'.replace(/[x]/g, function(c) {
            var r = uu[i].toString(16);
            r = (r.length === 1) ? '0' + r : r; // Zero pad single digit hex values
            i++;
            return r;
        });
        this.string = uuid;
    }
    return this.string;
};

/**
 * Compare two instances of proton.Data.Uuid for equality.
 * @method equals
 * @memberof! proton.Data.Uuid#
 * @param {proton.Data.Uuid} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Uuid'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};

