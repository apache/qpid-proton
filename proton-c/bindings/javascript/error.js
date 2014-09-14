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
/*                                   Status                                  */
/*                                                                           */
/*****************************************************************************/

/**
 * Export Status Enum, avoiding minification.
 * @enum
 * @alias Status
 * @memberof proton
 */
Module['Status'] = {
    /** PN_STATUS_UNKNOWN */  'UNKNOWN':  0, // The tracker is unknown.
    /** PN_STATUS_PENDING */  'PENDING':  1, // The message is in flight.
                                             // For outgoing messages, use messenger.isBuffered()
                                             // to see if it has been sent or not.
    /** PN_STATUS_ACCEPTED */ 'ACCEPTED': 2, // The message was accepted.
    /** PN_STATUS_REJECTED */ 'REJECTED': 3, // The message was rejected.
    /** PN_STATUS_RELEASED */ 'RELEASED': 4, // The message was released.
    /** PN_STATUS_MODIFIED */ 'MODIFIED': 5, // The message was modified.
    /** PN_STATUS_ABORTED */  'ABORTED':  6, // The message was aborted.
    /** PN_STATUS_SETTLED */  'SETTLED':  7  // The remote party has settled the message.
};


/*****************************************************************************/
/*                                                                           */
/*                                   Error                                   */
/*                                                                           */
/*****************************************************************************/

/**
 * Export Error Enum, avoiding minification.
 * @enum
 * @alias Error
 * @memberof proton
 */
Module['Error'] = {
    /** PN_EOS */        'EOS':        -1,
    /** PN_ERR */        'ERR':        -2,
    /** PN_OVERFLOW */   'OVERFLOW':   -3,
    /** PN_UNDERFLOW */  'UNDERFLOW':  -4,
    /** PN_STATE_ERR */  'STATE_ERR':  -5,
    /** PN_ARG_ERR */    'ARG_ERR':    -6,
    /** PN_TIMEOUT */    'TIMEOUT':    -7,
    /** PN_INTR */       'INTR':       -8,
    /** PN_INPROGRESS */ 'INPROGRESS': -9
};


/*****************************************************************************/
/*                                                                           */
/*                               MessengerError                              */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.MessengerError instance.
 * @classdesc This class is a subclass of Error.
 * @constructor proton.MessengerError
 * @param {string} message the error message.
 */
Module['MessengerError'] = function(message) { // MessengerError constructor.
    this.name = "MessengerError";
    this.message = (message || "");
};

Module['MessengerError'].prototype = new Error();
Module['MessengerError'].prototype.constructor = Module['MessengerError'];

Module['MessengerError'].prototype.toString = function() {
    return this.name + ': ' + this.message
};


/*****************************************************************************/
/*                                                                           */
/*                                MessageError                               */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.MessageError instance.
 * @classdesc This class is a subclass of Error.
 * @constructor proton.MessageError
 * @param {string} message the error message.
 */
Module['MessageError'] = function(message) { // MessageError constructor.
    this.name = "MessageError";
    this.message = (message || "");
};

Module['MessageError'].prototype = new Error();
Module['MessageError'].prototype.constructor = Module['MessageError'];

Module['MessageError'].prototype.toString = function() {
    return this.name + ': ' + this.message
};


/*****************************************************************************/
/*                                                                           */
/*                                  DataError                                */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a proton.DataError instance.
 * @classdesc This class is a subclass of Error.
 * @constructor proton.DataError
 * @param {string} message the error message.
 */
Module['DataError'] = function(message) { // DataError constructor.
    this.name = "DataError";
    this.message = (message || "");
};

Module['DataError'].prototype = new Error();
Module['DataError'].prototype.constructor = Module['DataError'];

Module['DataError'].prototype.toString = function() {
    return this.name + ': ' + this.message
};

