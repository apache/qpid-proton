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
        CONNECTION,
        SESSION,
        LINK,
        DELIVERY,
        TRANSPORT;
    }

    public enum Type {
        CONNECTION_INIT(Category.CONNECTION, 1),
        CONNECTION_OPEN(Category.CONNECTION, 2),
        CONNECTION_REMOTE_OPEN(Category.CONNECTION, 3),
        CONNECTION_CLOSE(Category.CONNECTION, 4),
        CONNECTION_REMOTE_CLOSE(Category.CONNECTION, 5),
        CONNECTION_FINAL(Category.CONNECTION, 6),

        SESSION_INIT(Category.SESSION, 1),
        SESSION_OPEN(Category.SESSION, 2),
        SESSION_REMOTE_OPEN(Category.SESSION, 3),
        SESSION_CLOSE(Category.SESSION, 4),
        SESSION_REMOTE_CLOSE(Category.SESSION, 5),
        SESSION_FINAL(Category.SESSION, 6),

        LINK_INIT(Category.LINK, 1),
        LINK_OPEN(Category.LINK, 2),
        LINK_REMOTE_OPEN(Category.LINK, 3),
        LINK_CLOSE(Category.LINK, 4),
        LINK_REMOTE_CLOSE(Category.LINK, 5),
        LINK_FLOW(Category.LINK, 6),
        LINK_FINAL(Category.LINK, 7),

        DELIVERY(Category.DELIVERY, 1),
        TRANSPORT(Category.TRANSPORT, 1);

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

    Event copy();

}
