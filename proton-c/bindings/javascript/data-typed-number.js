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
/*                             proton.Data.TypedNumber                       */
/*                                                                           */
/*****************************************************************************/

// ---------------------- JavaScript Number Extensions ------------------------ 

Number.prototype['ubyte'] = function() {
    return new Data.TypedNumber('UBYTE', this);
};

Number.prototype['byte'] = function() {
    return new Data.TypedNumber('BYTE', this);
};

Number.prototype['ushort'] = function() {
    return new Data.TypedNumber('USHORT', this);
};

Number.prototype['short'] = function() {
    return new Data.TypedNumber('SHORT', this);
};

Number.prototype['uint'] = function() {
    return new Data.TypedNumber('UINT', this);
};

Number.prototype['int'] = function() {
    return new Data.TypedNumber('INT', this);
};

Number.prototype['ulong'] = function() {
    return new Data.TypedNumber('ULONG', this);
};

Number.prototype['long'] = function() {
    return new Data.TypedNumber('LONG', this);
};

Number.prototype['float'] = function() {
    return new Data.TypedNumber('FLOAT', this);
};

Number.prototype['double'] = function() {
    return new Data.TypedNumber('DOUBLE', this);
};

Number.prototype['char'] = function() {
    return new Data.TypedNumber('CHAR', this);
};

String.prototype['char'] = function() {
    return new Data.TypedNumber('CHAR', this.charCodeAt(0));
};

// ------------------------- proton.Data.TypedNumber -------------------------- 
/**
 * Create a proton.Data.TypedNumber.
 * @classdesc
 * This class is a simple wrapper class that allows a "type" to be recorded for
 * a number. The idea is that the JavaScript Number class is extended with extra
 * methods to allow numbers to be "modified" to TypedNumbers, so for example
 * 1.0.float() would modify 1.0 by returning a TypedNumber with type = FLOAT
 * and value = 1. The strings used for type correspond to the names of the Data
 * put* methods e.g. UBYTE, BYTE, USHORT, SHORT, UINT, INT, ULONG, LONG, FLOAT,
 * DOUBLE, CHAR so that the correct method to call can be derived from the type.
 * @constructor proton.Data.TypedNumber
 * @param {(string|number)} type the type of the Number either as a string or number.
 *        Stored internally as a string corresponding to one of the TypeNames.
 * @param {number} value the value of the Number.
 */
// Use dot notation as it is a "protected" inner class not exported from the closure.
Data.TypedNumber = function(type, value) { // Data.TypedNumber Constructor.
    this.type  = (typeof type === 'number') ? Data['TypeNames'][type] : type;
    this.value = value;
};

/**
 * @method toString
 * @memberof! proton.Data.TypedNumber#
 * @returns {string} the String form of a {@link proton.Data.TypedNumber}.
 */
Data.TypedNumber.prototype.toString = Data.TypedNumber.prototype.valueOf = function() {
    return +this.value;
};


