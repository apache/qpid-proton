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
/*                             proton.Data.Symbol                            */
/*                                                                           */
/*****************************************************************************/

/**
 * Create a proton.Data.Symbol.
 * @classdesc
 * This class represents an AMQP Symbol. This class is necessary primarily as a
 * way to enable us to distinguish between a native String and a Symbol in the
 * JavaScript type system.
 * @constructor proton.Data.Symbol
 * @param {string} s a symbol.
 */
Data['Symbol'] = function(s) { // Data.Symbol Constructor.
    this.value = s;
};

/**
 * @method toString
 * @memberof! proton.Data.Symbol#
 * @returns {string} the String form of a {@link proton.Data.Symbol}.
 */
Data['Symbol'].prototype.toString = Data['Symbol'].prototype.valueOf = function() {
    return this.value;
};

/**
 * Compare two instances of proton.Data.Symbol for equality.
 * @method equals
 * @memberof! proton.Data.Symbol#
 * @param {proton.Data.Symbol} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data['Symbol'].prototype['equals'] = function(rhs) {
    return this.toString() === rhs.toString();
};

