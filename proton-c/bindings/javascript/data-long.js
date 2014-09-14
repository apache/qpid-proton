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
/*                             proton.Data.Long                              */
/*                                                                           */
/*****************************************************************************/

/**
 * Create a proton.Data.Long.
 * @classdesc
 * This class represents a 64 bit Integer value. It is used primarily to pass and
 * return 64 bit Integer values to and from the emscripten compiled proton-c library.
 * This class is needed because JavaScript cannot natively represent 64 bit
 * Integers with sufficient accuracy.
 * @constructor proton.Data.Long
 * @param {number} low the least significant word.
 * @param {number} high the most significant word.
 */
// Use dot notation as it is a "protected" inner class not exported from the closure.
Data.Long = function(low, high) { // Data.Long Constructor.
    this.low  = low  | 0;  // force into 32 signed bits.
    this.high = high | 0;  // force into 32 signed bits.
};

// proton.Data.Long constants.
Data.Long.TWO_PWR_16_DBL_ = 1 << 16;
Data.Long.TWO_PWR_32_DBL_ = Data.Long.TWO_PWR_16_DBL_ * Data.Long.TWO_PWR_16_DBL_;
Data.Long.TWO_PWR_64_DBL_ = Data.Long.TWO_PWR_32_DBL_ * Data.Long.TWO_PWR_32_DBL_;
Data.Long.TWO_PWR_63_DBL_ = Data.Long.TWO_PWR_64_DBL_ / 2;
Data.Long.MAX_VALUE = new Data.Long(0xFFFFFFFF | 0, 0x7FFFFFFF | 0);
Data.Long.MIN_VALUE = new Data.Long(0, 0x80000000 | 0);
Data.Long.ZERO = new Data.Long(0, 0);
Data.Long.ONE  = new Data.Long(1, 0);

/**
 * @method fromNumber
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} an instance of {@link proton.Data.Long} created
 *          using a native JavaScript number.
 */
Data.Long.fromNumber = function(value) {
    if (isNaN(value) || !isFinite(value)) {
        return Data.Long.ZERO;
    } else if (value <= -Data.Long.TWO_PWR_63_DBL_) {
        return Data.Long.MIN_VALUE;
    } else if (value + 1 >= Data.Long.TWO_PWR_63_DBL_) {
        return Data.Long.MAX_VALUE;
    } else if (value < 0) {
        return Data.Long.fromNumber(-value).negate();
    } else {
      return new Data.Long(
          (value % Data.Long.TWO_PWR_32_DBL_) | 0,
          (value / Data.Long.TWO_PWR_32_DBL_) | 0);
    }
};

/**
 * Return the twos complement of this instance.
 * @method negate
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} the twos complement of this instance.
 */
Data.Long.prototype.negate = function() {
    if (this.equals(Data.Long.MIN_VALUE)) {
        return Data.Long.MIN_VALUE;
    } else {
        return this.not().add(Data.Long.ONE);
    }
};

/**
 * Add two instances of {@link proton.Data.Long}.
 * @method add
 * @memberof! proton.Data.Long#
 * @param {proton.Data.Long} rhs the instance we wish to add to this instance.
 * @returns {proton.Data.Long} the sum of this value and the rhs.
 */
Data.Long.prototype.add = function(rhs) {
    // Divide each number into 4 chunks of 16 bits, and then sum the chunks.

    var a48 = this.high >>> 16;
    var a32 = this.high & 0xFFFF;
    var a16 = this.low >>> 16;
    var a00 = this.low & 0xFFFF;

    var b48 = rhs.high >>> 16;
    var b32 = rhs.high & 0xFFFF;
    var b16 = rhs.low >>> 16;
    var b00 = rhs.low & 0xFFFF;

    var c48 = 0, c32 = 0, c16 = 0, c00 = 0;
    c00 += a00 + b00;
    c16 += c00 >>> 16;
    c00 &= 0xFFFF;
    c16 += a16 + b16;
    c32 += c16 >>> 16;
    c16 &= 0xFFFF;
    c32 += a32 + b32;
    c48 += c32 >>> 16;
    c32 &= 0xFFFF;
    c48 += a48 + b48;
    c48 &= 0xFFFF;
    return new Data.Long((c16 << 16) | c00, (c48 << 16) | c32);
};

/**
 * Return the complement of this instance.
 * @method not
 * @memberof! proton.Data.Long#
 * @returns {proton.Data.Long} the complement of this instance.
 */
Data.Long.prototype.not = function() {
    return new Data.Long(~this.low, ~this.high);
};

/**
 * Compare two instances of {@link proton.Data.Long} for equality.
 * @method equals
 * @memberof! proton.Data.Long#
 * @param {proton.Data.Long} rhs the instance we wish to compare this instance with.
 * @returns {boolean} true iff the two compared instances are equal.
 */
Data.Long.prototype.equals = function(other) {
    return (this.high == other.high) && (this.low == other.low);
};

/**
 * @method getHighBits
 * @memberof! proton.Data.Long#
 * @returns {number} the most significant word of a {@link proton.Data.Long}.
 */
Data.Long.prototype.getHighBits = function() {
    return this.high;
};

/**
 * @method getLowBits
 * @memberof! proton.Data.Long#
 * @returns {number} the least significant word of a {@link proton.Data.Long}.
 */
Data.Long.prototype.getLowBits = function() {
    return this.low;
};

/**
 * @method getLowBitsUnsigned
 * @memberof! proton.Data.Long#
 * @returns {number} the least significant word of a {@link proton.Data.Long}
 *          as an unsigned value.
 */
Data.Long.prototype.getLowBitsUnsigned = function() {
    return (this.low >= 0) ? this.low : Data.Long.TWO_PWR_32_DBL_ + this.low;
};

/**
 * @method toNumber
 * @memberof! proton.Data.Long#
 * @returns {number} a native JavaScript number (with possible loss of precision).
 */
Data.Long.prototype.toNumber = function() {
    return (this.high * Data.Long.TWO_PWR_32_DBL_) + this.getLowBitsUnsigned();
};

/**
 * @method toString
 * @memberof! proton.Data.Long#
 * @returns {string} the String form of a {@link proton.Data.Long}.
 */
Data.Long.prototype.toString = function() {
    return this.high + ':' + this.getLowBitsUnsigned();
};

