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
/*                             proton.Data.Array                             */
/*                                                                           */
/*****************************************************************************/

/**
 * TODO make this behave more like a native JavaScript Array: http://www.bennadel.com/blog/2292-extending-javascript-arrays-while-keeping-native-bracket-notation-functionality.htm
 * Create a proton.Data.Array.
 * @classdesc
 * This class represents an AMQP Array.
 * @constructor proton.Data.Array
 * @param {string|number} type the type of the Number either as a string or number.
 *        Stored internally as a string corresponding to one of the TypeNames.       
 * @param {Array|TypedArray} elements the Native JavaScript Array or TypedArray that we wish to serialise.
 * @param {object} descriptor an optional object describing the type.
 */
Data['Array'] = function(type, elements, descriptor) { // Data.Array Constructor.
    // This block caters for an empty Array or a Described empty Array.
    if (arguments.length < 2) {
        descriptor = type;
        type = 'NULL';
        elements = [];
    }

    this['type']  = (typeof type === 'number') ? Data['TypeNames'][type] : type;
    this['elements'] = elements;
    this['descriptor'] = descriptor;
};

/**
 * @method toString
 * @memberof! proton.Data.Array#
 * @returns {string} the String form of a {@link proton.Data.Array}.
 */
Data['Array'].prototype.toString = function() {
    var descriptor = (this['descriptor'] == null) ? '' : ':' + this['descriptor'];
    return this['type'] + 'Array' + descriptor + '[' + this['elements'] + ']';
};

/**
 * @method valueOf
 * @memberof! proton.Data.Array#
 * @returns {Array} the elements of the {@link proton.Data.Array}.
 */
Data['Array'].prototype.valueOf = function() {
    return this['elements'];
};

/**
 * Compare two instances of proton.Data.Array for equality. N.B. this method
 * compares the value of every Array element so its performance is O(n).
 * @method equals
 * @memberof! proton.Data.Array#
 * @param {proton.Data.Array} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Array'].prototype['equals'] = function(rhs) {
    if (rhs instanceof Data['Array'] &&
        // Check the string value of the descriptors.
        (('' + this['descriptor']) === ('' + rhs['descriptor'])) &&
        (this['type'] === rhs['type'])) {
        var elements = this['elements'];
        var relements = rhs['elements'];
        var length = elements.length;
        if (length === relements.length) {
            for (var i = 0; i < length; i++) {
                if (elements[i].valueOf() !== relements[i].valueOf()) {
                    return false;
                }
            }
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
};

