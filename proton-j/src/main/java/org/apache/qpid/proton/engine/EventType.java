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
 * Entry point for external libraries to add event types. Event types should be
 * <code>final static</code> fields. EventType instances are compared by
 * reference.
 * <p>
 * Event types are best described by an <code>enum</code> that implements the
 * {@link EventType} interface, see {@link Event.Type}.
 * 
 */
public interface EventType {

    /**
     * @return false if this particular EventType instance does not represent a
     *         real event type but a guard value, example: extra enum value for
     *         switch statements, see {@link Event.Type#NON_CORE_EVENT}
     */
    public boolean isValid();
}
