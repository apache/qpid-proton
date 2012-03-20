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
package proton;

import java.util.Iterator;


/**
 * Link
 *
 * @opt operations
 * @opt types
 *
 * @assoc 1 - n Delivery
 *
 * @todo make links able to exist independently from
 *       sessions/connections and allow to migrate
 *
 */

public interface Link extends Endpoint
{

    /**
     * transition local state to ACTIVE
     */
    public void attach();

    /**
     * transition local state to CLOSED
     */
    public void detach();

    /**
     * @param tag a tag for the delivery
     *
     * @return a Delivery object
     */
    public Delivery delivery(byte[] tag);

    /**
     * @return the unsettled deliveries for this link
     */
    public Iterator<Delivery> unsettled();

    /**
     * Advances the current delivery to the next delivery on the link.
     *
     * @return the next delivery or null if there is none
     */
    public Delivery next();

}
