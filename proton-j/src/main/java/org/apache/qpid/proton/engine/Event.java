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
package org.apache.qpid.proton.engine;


/**
 * Event
 *
 */

public interface Event
{
    public enum Category {
        PROTOCOL;
    }

    public enum Type {
        CONNECTION_REMOTE_STATE(Category.PROTOCOL, 1),
        CONNECTION_LOCAL_STATE(Category.PROTOCOL, 2),
        SESSION_REMOTE_STATE(Category.PROTOCOL, 3),
        SESSION_LOCAL_STATE(Category.PROTOCOL, 4),
        LINK_REMOTE_STATE(Category.PROTOCOL, 5),
        LINK_LOCAL_STATE(Category.PROTOCOL, 6),
        LINK_FLOW(Category.PROTOCOL, 7),
        DELIVERY(Category.PROTOCOL, 8),
        TRANSPORT(Category.PROTOCOL, 9);

        private int _opcode;
        private Category _category;

        private Type(Category c, int o)
        {
            this._category = c;
            this._opcode = o;
        }

        public Category getCategory()
        {
            return this._category;
        }
    }

    Category getCategory();

    Type getType();

    Connection getConnection();

    Session getSession();

    Link getLink();

    Delivery getDelivery();

    Transport getTransport();

}
