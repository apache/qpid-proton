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
/*                               Subscription                                */
/*                                                                           */
/*****************************************************************************/

/**
 * Constructs a Subscription instance.
 * @classdesc This class is a wrapper for Messenger's subscriptions.
 * Subscriptions should never be *directly* instantiated by client code only via
 * Messenger.subscribe() or Messenger.incomingSubscription(), so we declare the
 * constructor in the scope of the package and don't export it via Module.
 * @constructor Subscription                                                              
 */
var Subscription = function(subscription) { // Subscription Constructor.
    this._subscription = subscription;
};

/**
 * TODO Not sure exactly what pn_subscription_get_context does.
 * @method getContext
 * @memberof! Subscription#
 * @returns the Subscription's Context.
 */
Subscription.prototype['getContext'] = function() {
    return _pn_subscription_get_context(this._subscription);
};

/**
 * TODO Not sure exactly what pn_subscription_set_context does.
 * @method setContext
 * @memberof! Subscription#
 * @param context the Subscription's new Context.
 */
Subscription.prototype['setContext'] = function(context) {
    _pn_subscription_set_context(this._subscription, context);
};

/**
 * @method getAddress
 * @memberof! Subscription#
 * @returns the Subscription's Address.
 */
Subscription.prototype['getAddress'] = function() {
    return Pointer_stringify(_pn_subscription_address(this._subscription));
};

