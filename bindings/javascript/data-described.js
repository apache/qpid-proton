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
/*                             proton.Data.Described                         */
/*                                                                           */
/*****************************************************************************/

/**
 * Create a proton.Data.Described.
 * @classdesc
 * This class represents an AMQP Described.
 * @constructor proton.Data.Described
 * @param {object} value the value of the described type.
 * @param {string} descriptor an optional string describing the type.
 * @property {object} value the actual value of the described type.
 * @property {string} descriptor a string describing the type.
 */
Data['Described'] = function(value, descriptor) { // Data.Described Constructor.
    this['value'] = value;
    this['descriptor'] = descriptor;
};

/**
 * @method toString
 * @memberof! proton.Data.Described#
 * @returns {string} the String form of a {@link proton.Data.Described}.
 */
Data['Described'].prototype.toString = function() {
    return 'Described(' + this['value'] + ', ' + this['descriptor'] + ')';
};

/**
 * @method valueOf
 * @memberof! proton.Data.Described#
 * @returns {object} the value of the {@link proton.Data.Described}.
 */
Data['Described'].prototype.valueOf = function() {
    return this['value'];
};

/**
 * Compare two instances of proton.Data.Described for equality.
 * @method equals
 * @memberof! proton.Data.Described#
 * @param {proton.Data.Described} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Described'].prototype['equals'] = function(rhs) {
    if (rhs instanceof Data['Described']) {
        return ((this['descriptor'] === rhs['descriptor']) && (this['value'] === rhs['value']));
    } else {
        return false;
    }
};

